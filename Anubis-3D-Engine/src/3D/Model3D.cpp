#include "3D/Model3D.hpp"
#include "3D/Scene.hpp"
#include "Time.hpp"
#include "Cursor.hpp"
#include "BinaryReader.hpp"
#include "DebugLog.hpp"
#include "3D/Skeleton3D.hpp"
#include "JsonReader.hpp"
#include <unordered_set>
#include <set>
namespace Anubis {

	int Model3D::_nextID = 0;

	Model3D::Model3D(float x, float y, float z, Quaternion rotation, float scale) {

		_id = _nextID++;

		_x = x;
		_y = y;
		_z = z;
		_rotation = rotation;
		_scale = scale;
		_isSelectable = false;

		_meshes.clear();

		offset = sf::Vector2f(0.0f, 0.0f);
		_lastRotationMouse = sf::Vector2f(Anubis::cursor->_position);
	}

	Model3D::~Model3D() {

	}

	void Model3D::addMesh(std::shared_ptr<Mesh3D> mesh) {
		_meshes.push_back(mesh);
	}

	bool Model3D::applyAnimationPose() {
		if (_animationIndex < 0 || static_cast<std::size_t>(_animationIndex) >= _animations.size()) {
			DebugError(L"Animation index is out of range");
			return false;
		}

		const auto& animation = _animations[_animationIndex];

		// Restore base transforms for properties without animation channels.
		for (auto& node : _nodes) {
			node._poseTranslation = node._translation;
			node._poseRotation = node._rotation;
			node._poseScale = node._scale;
		}

		for (const auto& channel : animation._channels) {
			if (channel._nodeIndex < 0 ||
				static_cast<std::size_t>(channel._nodeIndex) >= _nodes.size() ||
				channel._samplerIndex < 0 ||
				static_cast<std::size_t>(channel._samplerIndex) >=
				animation._samplers.size()) {
				DebugError(L"Animation channel index is out of range");
				return false;
			}

			auto& node = _nodes[channel._nodeIndex];
			const auto& sampler = animation._samplers[channel._samplerIndex];

			std::array<float, 4> value{};

			if (!sampleAnimationChannel(sampler, channel._path, _animationTime, value)) {
				return false;
			}

			switch (channel._path) {
			case AnimationPath::Translation:
				node._poseTranslation = { value[0], value[1], value[2] };
				break;

			case AnimationPath::Rotation:
				// glTF stores quaternion components in XYZW order.
				node._poseRotation = Quaternion{
					value[3], value[0], value[1], value[2]
				};
				break;

			case AnimationPath::Scale:
				node._poseScale = { value[0], value[1], value[2] };
				break;
			}
		}

		// Build local matrices using T * R * S.
		for (auto& node : _nodes) {
			if (node._hasMatrix) {
				node._localMatrix = node._matrix;
				continue;
			}

			auto matrix = node._poseRotation.matrix();

			for (int row = 0; row < 3; ++row) {
				matrix[row] *= node._poseScale[0];
				matrix[4 + row] *= node._poseScale[1];
				matrix[8 + row] *= node._poseScale[2];
			}

			matrix[12] = node._poseTranslation[0];
			matrix[13] = node._poseTranslation[1];
			matrix[14] = node._poseTranslation[2];

			node._localMatrix = matrix;
		}

		// The loader has already validated the hierarchy.
		// Process parents before their children.
		std::vector<int> pending;
		pending.reserve(_nodes.size());

		for (std::size_t i = 0; i < _nodes.size(); ++i) {
			if (_nodes[i]._parent == -1)
				pending.push_back(static_cast<int>(i));
		}

		for (std::size_t i = 0; i < pending.size(); ++i) {
			auto& node = _nodes[pending[i]];

			if (node._parent == -1) {
				node._globalMatrix = node._localMatrix;
			}
			else {
				const auto& parent = _nodes[node._parent]._globalMatrix;
				const auto& local = node._localMatrix;

				Matrix4 global{};

				for (int column = 0; column < 4; ++column) {
					for (int row = 0; row < 4; ++row) {
						for (int k = 0; k < 4; ++k) {
							global[column * 4 + row] +=
								parent[k * 4 + row] *
								local[column * 4 + k];
						}
					}
				}

				node._globalMatrix = global;
			}

			for (const int child : node._children)
				pending.push_back(child);
		}

		for (const auto& mesh : _meshes) {
			if (!mesh)
				continue;

			// Generated meshes may have no glTF node.
			if (mesh->_nodeIndex < 0)
				continue;

			if (!uploadMeshSkinPose(*this, *mesh))
				return false;
		}

		return true;
	}

	bool Model3D::playAnimation(int index, bool loop) {
		if (index < 0 || static_cast<std::size_t>(index) >= _animations.size()) {
			DebugError(L"Animation index is out of range");
			return false;
		}

		_animationIndex = index;
		_animationTime = 0.0;
		_animationLoop = loop;

		// Apply the starting pose immediately.
		if (!applyAnimationPose()) {
			_animationIndex = -1;
			return false;
		}

		return true;
	}

	bool Model3D::updateAnimation(float deltaSeconds) {
		if (_animationIndex < 0)
			return true;

		if (static_cast<std::size_t>(_animationIndex) >= _animations.size()) {
			DebugError(L"Animation index is out of range");
			return false;
		}

		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.f || !std::isfinite(_animationSpeed) || !std::isfinite(_animationTime)) {
			DebugError(L"Invalid animation playback time or speed");
			return false;
		}

		const double duration = _animations[_animationIndex]._duration;

		if (!std::isfinite(duration) || duration < 0.0) {
			DebugError(L"Invalid animation duration");
			return false;
		}

		//DebugStat(L"Animation time: " + std::to_wstring(_animationTime) + L", delta: " + std::to_wstring(deltaSeconds) + L", duration: " + std::to_wstring(duration));

		const double nextTime = _animationTime + double(deltaSeconds) * _animationSpeed;

		if (!std::isfinite(nextTime)) {
			DebugError(L"Animation playback time overflow");
			return false;
		}

		if (duration == 0.0) {
			_animationTime = 0.0;
		}
		else if (_animationLoop) {
			_animationTime = std::fmod(nextTime, duration);

			if (_animationTime < 0.0)
				_animationTime += duration;
		}
		else {
			_animationTime = std::clamp(nextTime, 0.0, duration);
		}

		return applyAnimationPose();
	}

	void Model3D::handleEvent(const sf::Event& event) {

		if (!_isSelectable)
			return;

		auto size = Anubis::render_window->getSize();
		auto mouse = Anubis::cursor->_position;

		if (size.x == 0 || size.y == 0)
			return;

		auto camera = scene->_camera;

		float sinAngle = std::sin(scene->_cameraAngle);
		float cosAngle = std::cos(scene->_cameraAngle);

		float relativeX = _x - camera.x;
		float relativeY = _y - camera.y;
		float relativeZ = _z - camera.z;

		float modelViewX = relativeX * cosAngle - relativeZ * sinAngle;
		float modelViewY = relativeY;

		float distance = -relativeX * sinAngle - relativeZ * cosAngle;
		float aspect = float(size.x) / float(size.y);

		float halfHeight = distance * std::tan(30.f * 3.14159265f / 180.f);
		float halfWidth = halfHeight * aspect;

		float mouseX = (2.f * mouse.x / size.x - 1.f) * halfWidth;
		float mouseY = (1.f - 2.f * mouse.y / size.y) * halfHeight;

		auto hovered = std::find(scene->_hovered.begin(), scene->_hovered.end(), this->shared_from_this());
		auto pressed = std::find(scene->_pressed.begin(), scene->_pressed.end(), this->shared_from_this());

		if (hovered != scene->_hovered.end()) {
			if (const auto* mbp = event.getIf<sf::Event::MouseButtonPressed>(); mbp && mbp->button == sf::Mouse::Button::Left) {

				if (std::find(
					scene->_pressed.begin(), scene->_pressed.end(), this->shared_from_this()
				) == scene->_pressed.end()) {
					scene->_pressed.push_back(this->shared_from_this());
				}
			}
		}

		pressed = std::find(scene->_pressed.begin(), scene->_pressed.end(), this->shared_from_this());

		if (pressed != scene->_pressed.end()) {
			if (const auto* mbp = event.getIf<sf::Event::MouseButtonPressed>(); mbp) {
				if (mbp->button == sf::Mouse::Button::Left) {
					offset.x = mouseX - modelViewX;
					offset.y = mouseY - modelViewY;
				}

				if (mbp->button == sf::Mouse::Button::Right) {
					_lastRotationMouse = sf::Vector2f(mbp->position);
				}
			}
		}

		if (pressed != scene->_pressed.end()) {
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && cursor->_isDragging) {
				float viewX = mouseX - offset.x;
				float viewY = mouseY - offset.y;

				_x = camera.x + viewX * cosAngle - distance * sinAngle;
				_y = camera.y + viewY;
				_z = camera.z - viewX * sinAngle - distance * cosAngle;
			}
		}
	}

	void Model3D::update() {

		if (!updateAnimation(Anubis::deltaTime.asSeconds())) {
			_animationIndex = -1;
			return;
		}

		// Play the next animation after the current one finishes.
		if (_animationIndex >= 0 && _animationTime >= _animations[_animationIndex]._duration) {

			if (_animationIndex + 1 < _animations.size()) {
				if (!playAnimation(_animationIndex + 1, false)) {
					_animationIndex = -1;
					return;
				}
			}
			else {
				_animationIndex = -1;
			}
		}

		if (!_isSelectable)
			return;

		auto pressed = std::find(scene->_pressed.begin(), scene->_pressed.end(), this->shared_from_this());

		if (pressed != scene->_pressed.end() && sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
		{
			auto delta = sf::Vector2f(Anubis::cursor->_position) - _lastRotationMouse;
			_lastRotationMouse = sf::Vector2f(Anubis::cursor->_position);

			float dx = (float)(delta.x);
			float dy = (float)(delta.y);
			float length = std::hypot(dx, dy);
			if (length > 0.f) {
				float sensitivity = 0.5f; // Stopnie na piksel.
				float toRadians = 3.14159265f / 180.f;

				float halfAngle = length * sensitivity * toRadians * 0.5f;
				float s = std::sin(halfAngle) / length;

				float sinAngle = std::sin(scene->_cameraAngle);
				float cosAngle = std::cos(scene->_cameraAngle);

				Quaternion deltaRotation{
					std::cos(halfAngle),
					dy * s * cosAngle,
					dx * s,
					-dy * s * sinAngle
				};

				_rotation = deltaRotation * _rotation;
				_rotation.normalize();
			}

			return;
		}

		// rotation
		const auto& q = _rotationDelta;
		float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);

		if (length > 0.f) {
			float halfAngle = std::atan2(length, q.w) * _rotationSpeed * Anubis::deltaTime.asSeconds();
			float s = std::sin(halfAngle) / length;

			Quaternion delta{
				std::cos(halfAngle),
				q.x * s,
				q.y * s,
				q.z * s
			};

			_rotation = delta * _rotation;
			_rotation.normalize();
		}
	}

	void Model3D::drawShape() {
		for (auto& mesh : _meshes) {
			mesh->drawShape(_x, _y, _z, _rotation, _scale);
		}
	}

	void Model3D::draw() {

		auto hovered = std::find(scene->_hovered.begin(), scene->_hovered.end(), this->shared_from_this());
		auto pressed = std::find(scene->_pressed.begin(), scene->_pressed.end(), this->shared_from_this());

		for (auto& mesh : _meshes) {
			bool highlighting = pressed != scene->_pressed.end() || (scene->_pressed.empty() && hovered != scene->_hovered.end());
			mesh->draw(highlighting, _x, _y, _z, _rotation, _scale);
		}
	}

	static bool loadModelNodes(JsonReader& reader, Model3D& model) {
		if (!reader.findRootField("nodes"))
			return false;

		if (!reader.expect('['))
			return false;

		std::vector<Node3D> nodes;

		const auto readFloatArray = [&](auto& values) -> bool {
			if (!reader.expect('['))
				return false;

			for (std::size_t i = 0; i < values.size(); ++i) {
				if (i > 0 && !reader.expect(','))
					return false;

				if (!reader.readFloat(values[i]))
					return false;
			}

			return reader.expect(']');
			};

		if (reader.peek() == ']') {
			DebugError(L"Model node array is empty");
			return false;
		}

		while (true) {
			if (!reader.expect('{'))
				return false;

			Node3D node;
			bool hasTRS = false;

			if (reader.peek() != '}') {
				while (true) {
					if (reader.peek() != '"') {
						DebugError(L"Expected a node property name");
						return false;
					}

					const std::string field = reader.readString();

					if (!reader.expect(':'))
						return false;

					if (field == "name") {
						if (reader.peek() != '"') {
							DebugError(L"Expected a node name string");
							return false;
						}

						node._name = reader.readString();
					}
					else if (field == "mesh") {
						if (!reader.readInt(node._mesh))
							return false;
					}
					else if (field == "skin") {
						if (!reader.readInt(node._skin))
							return false;
					}
					else if (field == "translation") {
						if (!readFloatArray(node._translation))
							return false;

						hasTRS = true;
					}
					else if (field == "scale") {
						if (!readFloatArray(node._scale))
							return false;

						hasTRS = true;
					}
					else if (field == "rotation") {
						std::array<float, 4> rotation{};

						if (!readFloatArray(rotation))
							return false;

						// Convert glTF XYZW order to Quaternion WXYZ.
						node._rotation = Quaternion{
							rotation[3],
							rotation[0],
							rotation[1],
							rotation[2]
						};

						const auto& q = node._rotation;

						const double lengthSquared =
							double(q.w) * q.w +
							double(q.x) * q.x +
							double(q.y) * q.y +
							double(q.z) * q.z;

						if (std::abs(lengthSquared - 1.0) > 0.001) {
							DebugError(L"Node rotation is not a unit quaternion");
							return false;
						}

						node._rotation.normalize();
						hasTRS = true;
					}
					else if (field == "matrix") {
						if (!readFloatArray(node._matrix))
							return false;

						node._hasMatrix = true;
					}
					else if (field == "children") {
						if (!reader.expect('['))
							return false;

						if (reader.peek() == ']') {
							DebugError(L"Node children array is empty");
							return false;
						}

						while (true) {
							int child = -1;

							if (!reader.readInt(child))
								return false;

							node._children.push_back(child);

							if (reader.peek() == ']')
								break;

							if (!reader.expect(','))
								return false;
						}

						if (!reader.expect(']'))
							return false;
					}
					else {
						// Skip properties not used by this loader.
						if (!reader.skipValue())
							return false;
					}

					if (reader.peek() == '}')
						break;

					if (!reader.expect(','))
						return false;
				}
			}

			if (!reader.expect('}'))
				return false;

			if (node._hasMatrix && hasTRS) {
				DebugError(L"Node contains both matrix and TRS properties");
				return false;
			}

			if (node._skin >= 0 && node._mesh < 0) {
				DebugError(L"Node references a skin without a mesh");
				return false;
			}

			if (node._hasMatrix) {
				node._localMatrix = node._matrix;
			}
			else {
				// Build the local matrix as T * R * S.
				node._localMatrix = node._rotation.matrix();

				for (int column = 0; column < 3; ++column) {
					for (int row = 0; row < 3; ++row) {
						node._localMatrix[column * 4 + row] *=
							node._scale[column];
					}
				}

				node._localMatrix[12] = node._translation[0];
				node._localMatrix[13] = node._translation[1];
				node._localMatrix[14] = node._translation[2];
			}

			// Initialize the current pose from the default transform.
			node._poseTranslation = node._translation;
			node._poseRotation = node._rotation;
			node._poseScale = node._scale;

			nodes.push_back(std::move(node));

			if (reader.peek() == ']')
				break;

			if (!reader.expect(','))
				return false;
		}

		if (!reader.expect(']'))
			return false;

		// Build parent links and validate child indices.
		for (std::size_t parent = 0; parent < nodes.size(); ++parent) {
			for (const int child : nodes[parent]._children) {
				if (child < 0 ||
					static_cast<std::size_t>(child) >= nodes.size()) {
					DebugError(L"Node child index is out of range");
					return false;
				}

				if (static_cast<std::size_t>(child) == parent) {
					DebugError(L"Node references itself as a child");
					return false;
				}

				if (nodes[child]._parent != -1) {
					DebugError(L"Node has multiple parents or duplicate child references");
					return false;
				}

				nodes[child]._parent = static_cast<int>(parent);
			}
		}

		// Traverse roots first to detect cycles and compute global matrices.
		std::vector<std::size_t> pending;

		for (std::size_t i = 0; i < nodes.size(); ++i) {
			if (nodes[i]._parent == -1)
				pending.push_back(i);
		}

		std::size_t visited = 0;

		while (!pending.empty()) {
			const auto index = pending.back();
			pending.pop_back();

			auto& node = nodes[index];

			if (node._parent == -1) {
				node._globalMatrix = node._localMatrix;
			}
			else {
				const auto& parentMatrix =
					nodes[node._parent]._globalMatrix;

				Matrix4 global{};

				// Multiply column-major matrices: parent * local.
				for (int column = 0; column < 4; ++column) {
					for (int row = 0; row < 4; ++row) {
						for (int k = 0; k < 4; ++k) {
							global[column * 4 + row] +=
								parentMatrix[k * 4 + row] *
								node._localMatrix[column * 4 + k];
						}
					}
				}

				node._globalMatrix = global;
			}

			++visited;

			for (const int child : node._children)
				pending.push_back(static_cast<std::size_t>(child));
		}

		if (visited != nodes.size()) {
			DebugError(L"Cycle detected in model node hierarchy");
			return false;
		}

		// Commit only after successful parsing and validation.
		model._nodes = std::move(nodes);

		DebugStat(L"Loaded model nodes: " + std::to_wstring(model._nodes.size()));

		return true;
	}

	static bool loadInverseBindMatrices(JsonReader& reader, const std::vector<std::uint8_t>& binData, int accessorIndex, Skin3D& skin) {
		if (!reader.findRootField("accessors") || !reader.enterArrayElement(accessorIndex) || !reader.expect('{')) {
			return false;
		}

		const int accessorStart = reader._position;

		// Read a property from the beginning of an object.
		const auto readIntField = [&](
			int objectStart,
			const std::string& name,
			int& value,
			bool required
			) -> bool {
				reader._position = objectStart;

				if (!reader.findField(name, required))
					return !required;

				return reader.readInt(value);
			};

		reader._position = accessorStart;

		if (reader.findField("sparse", false)) {
			DebugError(L"Sparse inverse bind matrices are not supported yet");
			return false;
		}

		int componentType = 0;
		int count = 0;
		int bufferView = -1;
		int accessorOffset = 0;

		if (!readIntField(
			accessorStart, "componentType", componentType, true) ||
			!readIntField(
				accessorStart, "count", count, true) ||
			!readIntField(
				accessorStart, "bufferView", bufferView, true) ||
			!readIntField(
				accessorStart, "byteOffset", accessorOffset, false)) {
			return false;
		}

		reader._position = accessorStart;

		if (!reader.findField("type"))
			return false;

		if (componentType != 5126 || reader.readString() != "MAT4") {
			DebugError(L"Inverse bind matrices must use FLOAT MAT4");
			return false;
		}

		if (count <= 0 || static_cast<std::size_t>(count) < skin._joints.size()) {
			DebugError(L"Not enough inverse bind matrices");
			return false;
		}

		if (!reader.findRootField("bufferViews") ||
			!reader.enterArrayElement(bufferView) ||
			!reader.expect('{')) {
			return false;
		}

		const int viewStart = reader._position;

		int buffer = -1;
		int viewOffset = 0;
		int viewLength = 0;

		if (!readIntField(viewStart, "buffer", buffer, true) ||
			!readIntField(viewStart, "byteOffset", viewOffset, false) ||
			!readIntField(viewStart, "byteLength", viewLength, true)) {
			return false;
		}

		if (buffer != 0) {
			DebugError(L"Only the embedded GLB buffer is supported");
			return false;
		}

		reader._position = viewStart;

		if (reader.findField("byteStride", false)) {
			DebugError(L"Inverse bind matrix buffer view must not use byteStride");
			return false;
		}

		constexpr std::uint64_t matrixBytes = 16 * sizeof(float);

		const auto viewBegin = static_cast<std::uint64_t>(viewOffset);
		const auto viewSize = static_cast<std::uint64_t>(viewLength);
		const auto localOffset = static_cast<std::uint64_t>(accessorOffset);
		const auto dataSize = static_cast<std::uint64_t>(count) * matrixBytes;

		if (viewBegin > binData.size() ||
			viewSize > binData.size() - viewBegin) {
			DebugError(L"Inverse bind matrix buffer view exceeds BIN bounds");
			return false;
		}

		if (localOffset > viewSize ||
			dataSize > viewSize - localOffset) {
			DebugError(L"Inverse bind matrices exceed buffer view bounds");
			return false;
		}

		if (localOffset % sizeof(float) != 0 ||
			(viewBegin + localOffset) % sizeof(float) != 0) {
			DebugError(L"Inverse bind matrices are not properly aligned");
			return false;
		}

		std::vector<Matrix4> matrices(skin._joints.size());

		for (std::size_t i = 0; i < matrices.size(); ++i) {
			const auto offset =
				viewBegin + localOffset + i * matrixBytes;

			std::memcpy(
				matrices[i].data(),
				binData.data() + static_cast<std::size_t>(offset),
				static_cast<std::size_t>(matrixBytes)
			);

			for (const float value : matrices[i]) {
				if (!std::isfinite(value)) {
					DebugError(L"Inverse bind matrix contains a non-finite value");
					return false;
				}
			}

			const auto& matrix = matrices[i];

			// The last row of an affine matrix is [0, 0, 0, 1].
			if (std::abs(matrix[3]) > 0.00001f ||
				std::abs(matrix[7]) > 0.00001f ||
				std::abs(matrix[11]) > 0.00001f ||
				std::abs(matrix[15] - 1.f) > 0.00001f) {
				DebugError(L"Invalid inverse bind matrix last row");
				return false;
			}
		}

		skin._inverseBindMatrices = std::move(matrices);
		return true;
	}

	static bool loadModelSkins(JsonReader& reader, const std::vector<std::uint8_t>& binData, Model3D& model) {
		reader._position = 0;

		if (!reader.expect('{'))
			return false;

		// Static models may have no skins.
		if (!reader.findField("skins", false)) {
			for (const auto& node : model._nodes) {
				if (node._skin >= 0) {
					DebugError(L"Node references a skin but the skins array is missing");
					return false;
				}
			}

			model._skins.clear();
			DebugStat(L"Model has no skins");
			return true;
		}

		if (!reader.expect('['))
			return false;

		if (reader.peek() == ']') {
			DebugError(L"Model skin array is empty");
			return false;
		}

		std::vector<Skin3D> skins;

		while (true) {
			if (!reader.expect('{'))
				return false;

			Skin3D skin;
			int inverseBindAccessor = -1;
			bool hasJoints = false;

			if (reader.peek() != '}') {
				while (true) {
					if (reader.peek() != '"') {
						DebugError(L"Expected a skin property name");
						return false;
					}

					const std::string field = reader.readString();

					if (!reader.expect(':'))
						return false;

					if (field == "name") {
						if (reader.peek() != '"') {
							DebugError(L"Expected a skin name string");
							return false;
						}

						skin._name = reader.readString();
					}
					else if (field == "skeleton") {
						if (!reader.readInt(skin._skeletonRoot))
							return false;
					}
					else if (field == "inverseBindMatrices") {
						if (!reader.readInt(inverseBindAccessor))
							return false;
					}
					else if (field == "joints") {
						if (hasJoints) {
							DebugError(L"Duplicate skin joints property");
							return false;
						}

						hasJoints = true;

						if (!reader.expect('['))
							return false;

						if (reader.peek() == ']') {
							DebugError(L"Skin joint array is empty");
							return false;
						}

						std::unordered_set<int> uniqueJoints;

						while (true) {
							int joint = -1;

							if (!reader.readInt(joint))
								return false;

							if (static_cast<std::size_t>(joint) >=
								model._nodes.size()) {
								DebugError(L"Skin joint node index is out of range");
								return false;
							}

							if (!uniqueJoints.insert(joint).second) {
								DebugError(L"Skin contains a duplicate joint");
								return false;
							}

							skin._joints.push_back(joint);

							if (reader.peek() == ']')
								break;

							if (!reader.expect(','))
								return false;
						}

						if (!reader.expect(']'))
							return false;
					}
					else {
						if (!reader.skipValue())
							return false;
					}

					if (reader.peek() == '}')
						break;

					if (!reader.expect(','))
						return false;
				}
			}

			if (!reader.expect('}'))
				return false;

			if (!hasJoints) {
				DebugError(L"Skin joints property is missing");
				return false;
			}

			if (skin._skeletonRoot >= 0) {
				if (static_cast<std::size_t>(skin._skeletonRoot) >=
					model._nodes.size()) {
					DebugError(L"Skeleton root node index is out of range");
					return false;
				}

				// The specified skeleton root must contain every joint.
				for (const int joint : skin._joints) {
					int ancestor = joint;

					while (ancestor != -1 &&
						ancestor != skin._skeletonRoot) {
						ancestor = model._nodes[ancestor]._parent;
					}

					if (ancestor == -1) {
						DebugError(L"Skeleton root is not an ancestor of every joint");
						return false;
					}
				}
			}

			// Accessor reading moves the JSON cursor to another root field.
			const int nextSkinPosition = reader._position;

			if (inverseBindAccessor >= 0) {
				if (!loadInverseBindMatrices(
					reader, binData, inverseBindAccessor, skin
				)) {
					return false;
				}
			}
			else {
				skin._inverseBindMatrices.assign(
					skin._joints.size(),
					identityMatrix4()
				);
			}

			reader._position = nextSkinPosition;

			skins.push_back(std::move(skin));

			if (reader.peek() == ']')
				break;

			if (!reader.expect(','))
				return false;
		}

		if (!reader.expect(']'))
			return false;

		for (const auto& node : model._nodes) {
			if (node._skin >= 0 &&
				static_cast<std::size_t>(node._skin) >= skins.size()) {
				DebugError(L"Node skin index is out of range");
				return false;
			}
		}

		model._skins = std::move(skins);

		DebugStat(
			L"Loaded model skins: " +
			std::to_wstring(model._skins.size())
		);

		for (std::size_t i = 0; i < model._skins.size(); ++i) {
			DebugStat(
				L"Skin " + std::to_wstring(i) +
				L": joints = " +
				std::to_wstring(model._skins[i]._joints.size())
			);
		}

		return true;
	}

	static bool loadSkinAttribute(JsonReader& reader, const std::vector<std::uint8_t>& binData, int accessorIndex, std::size_t vertexCount, bool joints, std::vector<std::array<float, 4>>& values) {
		
		if (!reader.findRootField("accessors") ||
			!reader.enterArrayElement(accessorIndex) ||
			!reader.expect('{')) {
			return false;
		}

		const int accessorStart = reader._position;

		const auto readIntField = [&](
			int start,
			const std::string& name,
			int& value,
			bool required
			) -> bool {
				reader._position = start;

				if (!reader.findField(name, required))
					return !required;

				return reader.readInt(value);
			};

		reader._position = accessorStart;

		if (reader.findField("sparse", false)) {
			DebugError(L"Sparse skin attributes are not supported yet");
			return false;
		}

		int count = 0;
		int componentType = 0;
		int bufferView = -1;
		int accessorOffset = 0;

		if (!readIntField(accessorStart, "count", count, true) ||
			!readIntField(accessorStart, "componentType", componentType, true) ||
			!readIntField(accessorStart, "bufferView", bufferView, true) ||
			!readIntField(accessorStart, "byteOffset", accessorOffset, false)) {
			return false;
		}

		if (count <= 0 || static_cast<std::size_t>(count) != vertexCount) {
			DebugError(L"Skin attribute count does not match vertex count");
			return false;
		}

		reader._position = accessorStart;

		if (!reader.findField("type"))
			return false;

		if (reader.readString() != "VEC4") {
			DebugError(L"Skin attribute must use VEC4");
			return false;
		}

		bool normalized = false;
		reader._position = accessorStart;

		if (reader.findField("normalized", false)) {
			reader.skipWhitespace();
			const int start = reader._position;

			if (!reader.skipValue())
				return false;

			const auto token = reader._text.substr(
				start, reader._position - start
			);

			if (token == "true") {
				normalized = true;
			}
			else if (token != "false") {
				DebugError(L"Invalid normalized property");
				return false;
			}
		}

		int componentBytes = 0;

		switch (componentType) {
		case 5121:
			componentBytes = 1;
			break;

		case 5123:
			componentBytes = 2;
			break;

		case 5126:
			componentBytes = 4;
			break;

		default:
			DebugError(L"Unsupported skin attribute component type");
			return false;
		}

		if (joints) {
			if (componentType == 5126 || normalized) {
				DebugError(L"Joint indices must use non-normalized unsigned integers");
				return false;
			}
		}
		else {
			if ((componentType == 5126 && normalized) ||
				(componentType != 5126 && !normalized)) {
				DebugError(L"Weights must use floats or normalized unsigned integers");
				return false;
			}
		}

		if (!reader.findRootField("bufferViews") ||
			!reader.enterArrayElement(bufferView) ||
			!reader.expect('{')) {
			return false;
		}

		const int viewStart = reader._position;

		int buffer = -1;
		int viewOffset = 0;
		int viewLength = 0;
		int stride = componentBytes * 4;

		if (!readIntField(viewStart, "buffer", buffer, true) ||
			!readIntField(viewStart, "byteOffset", viewOffset, false) ||
			!readIntField(viewStart, "byteLength", viewLength, true) ||
			!readIntField(viewStart, "byteStride", stride, false)) {
			return false;
		}

		if (buffer != 0) {
			DebugError(L"Only the embedded GLB buffer is supported");
			return false;
		}

		const int elementBytes = componentBytes * 4;

		if (stride < elementBytes || stride > 252 || stride % 4 != 0) {
			DebugError(L"Invalid skin attribute byte stride");
			return false;
		}

		const auto viewBegin = static_cast<std::uint64_t>(viewOffset);
		const auto viewSize = static_cast<std::uint64_t>(viewLength);
		const auto localOffset = static_cast<std::uint64_t>(accessorOffset);

		const auto requiredBytes =
			static_cast<std::uint64_t>(count - 1) * stride + elementBytes;

		if (viewBegin > binData.size() ||
			viewSize > binData.size() - viewBegin) {
			DebugError(L"Skin attribute buffer view exceeds BIN bounds");
			return false;
		}

		if (localOffset > viewSize ||
			requiredBytes > viewSize - localOffset) {
			DebugError(L"Skin attribute exceeds buffer view bounds");
			return false;
		}

		if (localOffset % 4 != 0 ||
			(viewBegin + localOffset) % componentBytes != 0) {
			DebugError(L"Skin attribute is not properly aligned");
			return false;
		}

		values.resize(vertexCount);

		for (std::size_t i = 0; i < vertexCount; ++i) {
			const auto offset =
				viewBegin + localOffset +
				static_cast<std::uint64_t>(i) * stride;

			for (int component = 0; component < 4; ++component) {
				const auto* source = binData.data() +
					static_cast<std::size_t>(
						offset + component * componentBytes
						);

				float value = 0.f;

				if (componentType == 5121) {
					value = static_cast<float>(*source);

					if (normalized)
						value /= 255.f;
				}
				else if (componentType == 5123) {
					// Decode the little-endian unsigned short.
					const std::uint16_t integer =
						static_cast<std::uint16_t>(source[0]) |
						static_cast<std::uint16_t>(
							static_cast<std::uint16_t>(source[1]) << 8
							);

					value = static_cast<float>(integer);

					if (normalized)
						value /= 65535.f;
				}
				else {
					// The current target platform uses little-endian floats.
					std::memcpy(&value, source, sizeof(value));
				}

				if (!std::isfinite(value) || value < 0.f) {
					DebugError(L"Invalid skin attribute value");
					return false;
				}

				values[i][component] = value;
			}
		}

		return true;
	}

	static bool loadMeshSkinData(JsonReader& reader, const std::vector<std::uint8_t>& binData, const Model3D& model, Mesh3D& mesh) {
		// The current loader reads mesh 0, primitive 0.
		int nodeIndex = -1;

		for (std::size_t i = 0; i < model._nodes.size(); ++i) {
			if (model._nodes[i]._mesh != 0)
				continue;

			if (nodeIndex != -1) {
				DebugError(L"Multiple instances of mesh 0 are not supported yet");
				return false;
			}

			nodeIndex = static_cast<int>(i);
		}

		if (nodeIndex == -1) {
			DebugError(L"No node references mesh 0");
			return false;
		}

		const auto& node = model._nodes[nodeIndex];

		if (node._skin == -1) {
			mesh._nodeIndex = nodeIndex;
			DebugStat(L"Mesh has no skin");
			return true;
		}

		if (node._skin < 0 ||
			static_cast<std::size_t>(node._skin) >= model._skins.size()) {
			DebugError(L"Mesh skin index is out of range");
			return false;
		}

		const auto& skin = model._skins[node._skin];

		if (!reader.findRootField("meshes") ||
			!reader.enterArrayElement(0) ||
			!reader.expect('{') ||
			!reader.findField("primitives") ||
			!reader.enterArrayElement(0) ||
			!reader.expect('{') ||
			!reader.findField("attributes") ||
			!reader.expect('{')) {
			return false;
		}

		const int attributesStart = reader._position;

		// Reject additional influence sets instead of silently losing weights.
		for (const auto* field : { "JOINTS_1", "WEIGHTS_1" }) {
			reader._position = attributesStart;

			if (reader.findField(field, false)) {
				DebugError(L"More than four joint influences per vertex are not supported yet");
				return false;
			}
		}

		int jointsAccessor = -1;
		int weightsAccessor = -1;

		reader._position = attributesStart;

		if (!reader.findField("JOINTS_0") ||
			!reader.readInt(jointsAccessor)) {
			return false;
		}

		reader._position = attributesStart;

		if (!reader.findField("WEIGHTS_0") ||
			!reader.readInt(weightsAccessor)) {
			return false;
		}

		std::vector<std::array<float, 4>> jointValues;
		std::vector<std::array<float, 4>> weightValues;

		if (!loadSkinAttribute(
			reader, binData, jointsAccessor,
			mesh._vertices.size(), true, jointValues
		)) {
			return false;
		}

		if (!loadSkinAttribute(
			reader, binData, weightsAccessor,
			mesh._vertices.size(), false, weightValues
		)) {
			return false;
		}

		// Validate all vertices before modifying the mesh.
		for (std::size_t i = 0; i < mesh._vertices.size(); ++i) {
			double totalWeight = 0.0;

			for (int j = 0; j < 4; ++j) {
				const auto joint =
					static_cast<std::uint16_t>(jointValues[i][j]);

				if (joint >= skin._joints.size()) {
					DebugError(L"Vertex joint index exceeds skin joint count");
					return false;
				}

				totalWeight += weightValues[i][j];

				if (weightValues[i][j] > 0.f) {
					for (int previous = 0; previous < j; ++previous) {
						if (weightValues[i][previous] > 0.f &&
							jointValues[i][previous] == jointValues[i][j]) {
							DebugError(L"Vertex has duplicate active joint influences");
							return false;
						}
					}
				}
			}

			if (totalWeight <= 0.0 ||
				std::abs(totalWeight - 1.0) > 0.01) {
				DebugError(L"Vertex weights do not sum approximately to one");
				return false;
			}

			// Correct small rounding errors.
			for (float& weight : weightValues[i])
				weight = static_cast<float>(weight / totalWeight);
		}

		for (std::size_t i = 0; i < mesh._vertices.size(); ++i) {
			auto& vertex = mesh._vertices[i];

			for (int j = 0; j < 4; ++j) {
				vertex._joints[j] =
					static_cast<std::uint16_t>(jointValues[i][j]);

				vertex._weights[j] = weightValues[i][j];
			}
		}

		mesh._nodeIndex = nodeIndex;

		DebugStat(L"Mesh node index: " + std::to_wstring(nodeIndex));
		DebugStat(L"Mesh skin index: " + std::to_wstring(node._skin));
		DebugStat(L"Joints accessor: " + std::to_wstring(jointsAccessor));
		DebugStat(L"Weights accessor: " + std::to_wstring(weightsAccessor));

		DebugStat(
			L"Loaded skin data for vertices: " +
			std::to_wstring(mesh._vertices.size())
		);

		return true;
	}

	static bool uploadMeshSkinPose(const Model3D& model, Mesh3D& mesh) {
		if (mesh._nodeIndex < 0 || static_cast<std::size_t>(mesh._nodeIndex) >= model._nodes.size()) {
			DebugError(L"Mesh node index is out of range");
			return false;
		}

		const auto& node = model._nodes[mesh._nodeIndex];

		if (node._skin == -1)
			return true;

		if (node._skin < 0 || static_cast<std::size_t>(node._skin) >= model._skins.size()) {
			DebugError(L"Mesh skin index is out of range");
			return false;
		}

		const auto& skin = model._skins[node._skin];

		if (skin._inverseBindMatrices.size() != skin._joints.size()) {
			DebugError(L"Inverse bind matrix count does not match joint count");
			return false;
		}

		std::vector<Matrix4> matrices(skin._joints.size());

		for (std::size_t i = 0; i < skin._joints.size(); ++i) {
			const int jointNode = skin._joints[i];

			if (jointNode < 0 || static_cast<std::size_t>(jointNode) >= model._nodes.size()) {
				DebugError(L"Joint node index is out of range");
				return false;
			}

			const auto& global = model._nodes[jointNode]._globalMatrix;
			const auto& inverseBind = skin._inverseBindMatrices[i];

			Matrix4 result{};

			// Transform bind-space vertices into model space.
			for (int column = 0; column < 4; ++column) {
				for (int row = 0; row < 4; ++row) {
					for (int k = 0; k < 4; ++k) {
						result[column * 4 + row] +=
							global[k * 4 + row] *
							inverseBind[column * 4 + k];
					}
				}
			}

			matrices[i] = result;
		}

		return mesh.uploadJointMatrices(matrices);
	}

	static bool sampleAnimationChannel(const AnimationSampler3D& sampler, AnimationPath path, double time, std::array<float, 4>& result) {
		const auto& times = sampler._times;
		const auto& values = sampler._values;

		const bool cubic = sampler._interpolation == AnimationInterpolation::CubicSpline;

		const std::size_t multiplier = cubic ? 3 : 1;

		if (times.empty() ||
			values.size() / multiplier != times.size() ||
			values.size() % multiplier != 0) {
			DebugError(L"Invalid animation sampler data");
			return false;
		}

		const auto valueAt = [&](std::size_t key) {
			return values[cubic ? key * 3 + 1 : key];
		};

		// Clamp samples outside the channel's time range.
		if (time <= times.front()) {
			result = valueAt(0);
		}
		else if (time >= times.back()) {
			result = valueAt(times.size() - 1);
		}
		else {
			const auto next = std::upper_bound(times.begin(), times.end(), time);
			const std::size_t right = static_cast<std::size_t>(next - times.begin());
			const std::size_t left = right - 1;

			const float duration = times[right] - times[left];
			const float t = static_cast<float>((time - times[left]) / duration);

			auto a = valueAt(left);
			auto b = valueAt(right);

			if (sampler._interpolation == AnimationInterpolation::Step) {
				result = a;
			}
			else if (cubic) {
				const float t2 = t * t;
				const float t3 = t2 * t;

				const float h00 = 2.f * t3 - 3.f * t2 + 1.f;
				const float h10 = t3 - 2.f * t2 + t;
				const float h01 = -2.f * t3 + 3.f * t2;
				const float h11 = t3 - t2;

				const auto& outgoing = values[left * 3 + 2];
				const auto& incoming = values[right * 3];

				// Spline tangents are scaled by the segment duration.
				for (std::size_t c = 0; c < 4; ++c) {
					result[c] =
						h00 * a[c] +
						h10 * duration * outgoing[c] +
						h01 * b[c] +
						h11 * duration * incoming[c];
				}
			}
			else if (path == AnimationPath::Rotation) {
				// Normalize endpoints before spherical interpolation.
				float lengthA = 0.f;
				float lengthB = 0.f;

				for (std::size_t c = 0; c < 4; ++c) {
					lengthA += a[c] * a[c];
					lengthB += b[c] * b[c];
				}

				if (!(lengthA > 0.f) || !(lengthB > 0.f)) {
					DebugError(L"Invalid animation rotation endpoint");
					return false;
				}

				lengthA = std::sqrt(lengthA);
				lengthB = std::sqrt(lengthB);

				float dot = 0.f;

				for (std::size_t c = 0; c < 4; ++c) {
					a[c] /= lengthA;
					b[c] /= lengthB;
					dot += a[c] * b[c];
				}

				// Follow the shortest rotation path.
				if (dot < 0.f) {
					dot = -dot;

					for (float& value : b)
						value = -value;
				}

				dot = std::clamp(dot, 0.f, 1.f);

				float factorA = 1.f - t;
				float factorB = t;

				if (dot < 0.9995f) {
					const float angle = std::acos(dot);
					const float denominator = std::sin(angle);

					factorA = std::sin((1.f - t) * angle) / denominator;
					factorB = std::sin(t * angle) / denominator;
				}

				for (std::size_t c = 0; c < 4; ++c)
					result[c] = factorA * a[c] + factorB * b[c];
			}
			else {
				for (std::size_t c = 0; c < 4; ++c)
					result[c] = (1.f - t) * a[c] + t * b[c];
			}
		}

		for (const float value : result) {
			if (!std::isfinite(value)) {
				DebugError(L"Animation sample contains a non-finite value");
				return false;
			}
		}

		if (path == AnimationPath::Rotation) {
			double lengthSquared = 0.0;

			for (const float value : result)
				lengthSquared += double(value) * value;

			if (lengthSquared <= 0.0) {
				DebugError(L"Animation produced a zero quaternion");
				return false;
			}

			const double inverseLength = 1.0 / std::sqrt(lengthSquared);

			for (float& value : result)
				value = static_cast<float>(value * inverseLength);
		}

		return true;
	}

	static bool indexJsonObjectArray(JsonReader& reader, const std::string& field, std::vector<int>& positions) {
		if (!reader.findRootField(field) || !reader.expect('['))
			return false;

		positions.clear();

		if (reader.peek() == ']')
			return reader.expect(']');

		while (true) {
			if (reader.peek() != '{') {
				DebugError(L"Expected an object while indexing JSON");
				return false;
			}

			// Store the position of the opening brace.
			positions.push_back(reader._position);

			if (!reader.skipValue())
				return false;

			if (reader.peek() == ']')
				return reader.expect(']');

			if (!reader.expect(','))
				return false;
		}
	}

	static bool enterIndexedJsonObject(JsonReader& reader, const std::vector<int>& positions, int index) {
		if (index < 0 ||
			static_cast<std::size_t>(index) >= positions.size()) {
			DebugError(L"JSON object index is out of range");
			return false;
		}

		reader._position = positions[index];
		return reader.expect('{');
	}

	static bool loadAnimationFloatAccessor(JsonReader& reader, const std::vector<std::uint8_t>& binData, int accessorIndex, int expectedComponents, std::vector<std::array<float, 4>>& values, const AnimationJsonIndex& jsonIndex) {
		if (expectedComponents != 1 && expectedComponents != 3 && expectedComponents != 4) {
			DebugError(L"Invalid animation component count");
			return false;
		}

		if (!enterIndexedJsonObject(reader, jsonIndex._accessors, accessorIndex)) {
			return false;
		}

		const int accessorStart = reader._position;

		const auto readIntField = [&](int start, const std::string& name, int& value, bool required) -> bool {
				reader._position = start;
				if (!reader.findField(name, required))
					return !required;

				return reader.readInt(value);
			};

		reader._position = accessorStart;

		if (reader.findField("sparse", false)) {
			DebugError(L"Sparse animation accessors are not supported yet");
			return false;
		}

		int componentType = 0;
		int count = 0;
		int bufferView = -1;
		int accessorOffset = 0;

		if (!readIntField(accessorStart, "componentType", componentType, true) || !readIntField(accessorStart, "count", count, true) || !readIntField(accessorStart, "bufferView", bufferView, true) || !readIntField(accessorStart, "byteOffset", accessorOffset, false)) {
			return false;
		}

		if (componentType != 5126) {
			DebugError(L"Only FLOAT animation accessors are supported here");
			return false;
		}

		if (count <= 0) {
			DebugError(L"Animation accessor is empty");
			return false;
		}

		reader._position = accessorStart;

		if (reader.findField("normalized", false)) {
			reader.skipWhitespace();
			const int start = reader._position;

			if (!reader.skipValue())
				return false;

			const auto token = reader._text.substr(start, reader._position - start);

			if (token != "false") {
				DebugError(L"FLOAT animation accessor must not be normalized");
				return false;
			}
		}

		reader._position = accessorStart;

		if (!reader.findField("type"))
			return false;

		const std::string type = reader.readString();

		const std::string expectedType =
			expectedComponents == 1 ? "SCALAR" :
			expectedComponents == 3 ? "VEC3" : "VEC4";

		if (type != expectedType) {
			DebugError(L"Unexpected animation accessor type");
			return false;
		}

		if (!enterIndexedJsonObject(reader, jsonIndex._bufferViews, bufferView)) {
			return false;
		}

		const int viewStart = reader._position;

		int buffer = -1;
		int viewOffset = 0;
		int viewLength = 0;

		if (!readIntField(viewStart, "buffer", buffer, true) || !readIntField(viewStart, "byteOffset", viewOffset, false) || !readIntField(viewStart, "byteLength", viewLength, true)) {
			return false;
		}

		if (buffer != 0) {
			DebugError(L"Only the embedded GLB buffer is supported");
			return false;
		}

		reader._position = viewStart;

		if (reader.findField("byteStride", false)) {
			DebugError(L"Animation buffer view must not use byteStride");
			return false;
		}

		const auto elementBytes = static_cast<std::uint64_t>(expectedComponents) * sizeof(float);

		const auto dataBytes = static_cast<std::uint64_t>(count) * elementBytes;

		const auto viewBegin = static_cast<std::uint64_t>(viewOffset);
		const auto viewSize = static_cast<std::uint64_t>(viewLength);
		const auto localOffset = static_cast<std::uint64_t>(accessorOffset);

		if (viewBegin > binData.size() || viewSize > binData.size() - viewBegin) {
			DebugError(L"Animation buffer view exceeds BIN bounds");
			return false;
		}

		if (localOffset > viewSize || dataBytes > viewSize - localOffset) {
			DebugError(L"Animation accessor exceeds buffer view bounds");
			return false;
		}

		if (localOffset % sizeof(float) != 0 || (viewBegin + localOffset) % sizeof(float) != 0) {
			DebugError(L"Animation accessor is not properly aligned");
			return false;
		}

		std::vector<std::array<float, 4>> loaded(static_cast<std::size_t>(count), std::array<float, 4>{});

		for (std::size_t i = 0; i < loaded.size(); ++i) {
			const auto offset = viewBegin + localOffset + i * elementBytes;

			// The current target platform uses little-endian floats.
			std::memcpy(
				loaded[i].data(),
				binData.data() + static_cast<std::size_t>(offset),
				static_cast<std::size_t>(elementBytes)
			);

			for (int component = 0; component < expectedComponents; ++component) {
				if (!std::isfinite(loaded[i][component])) {
					DebugError(L"Animation data contains a non-finite value");
					return false;
				}
			}
		}

		// Commit only after successful validation.
		values = std::move(loaded);
		return true;
	}

	static bool loadModelAnimations(JsonReader& reader, const std::vector<std::uint8_t>& binData, Model3D& model) {
		// Store object positions to allow independent field lookup.
		const auto readObjectArray = [&](std::vector<int>& positions) -> bool {

			if (!reader.expect('['))
				return false;

			positions.clear();

			if (reader.peek() == ']')
				return reader.expect(']');

			while (true) {
				if (reader.peek() != '{') {
					DebugError(L"Expected an object in animation array");
					return false;
				}

				positions.push_back(reader._position);

				if (!reader.skipValue())
					return false;

				if (reader.peek() == ']')
					return reader.expect(']');

				if (!reader.expect(','))
					return false;
			}
		};

		const auto findObjectField = [&](int objectPosition, const std::string& name, bool required) -> bool {
			reader._position = objectPosition;

			if (!reader.expect('{'))
				return false;

			return reader.findField(name, required);
		};

		const auto readRequiredInt = [&](int objectPosition, const std::string& name, int& value) -> bool {
			if (!findObjectField(objectPosition, name, true))
				return false;

			return reader.readInt(value);
		};

		reader._position = 0;

		if (!reader.expect('{'))
			return false;

		if (!reader.findField("animations", false)) {
			model._animations.clear();
			model._animationIndex = -1;
			model._animationTime = 0.0;

			DebugStat(L"Model has no animations");
			return true;
		}

		std::vector<int> animationPositions;

		if (!readObjectArray(animationPositions))
			return false;

		if (animationPositions.empty()) {
			DebugError(L"Animation array is empty");
			return false;
		}

		AnimationJsonIndex jsonIndex;

		if (!indexJsonObjectArray(reader, "accessors", jsonIndex._accessors)) {
			return false;
		}

		if (!indexJsonObjectArray(reader, "bufferViews", jsonIndex._bufferViews)) {
			return false;
		}

		DebugStat(L"Indexed animation accessors: " + std::to_wstring(jsonIndex._accessors.size()));
		DebugStat(L"Indexed animation buffer views: " + std::to_wstring(jsonIndex._bufferViews.size()));

		std::vector<Animation3D> animations;
		animations.reserve(animationPositions.size());

		for (const int animationPosition : animationPositions) {
			Animation3D animation;

			if (findObjectField(animationPosition, "name", false)) {
				if (reader.peek() != '"') {
					DebugError(L"Expected an animation name string");
					return false;
				}

				animation._name = reader.readString();
			}

			std::vector<int> samplerPositions;
			std::vector<int> channelPositions;

			if (!findObjectField(animationPosition, "samplers", true) || !readObjectArray(samplerPositions)) {
				return false;
			}

			if (!findObjectField(animationPosition, "channels", true) || !readObjectArray(channelPositions)) {
				return false;
			}

			if (samplerPositions.empty() || channelPositions.empty()) {
				DebugError(L"Animation must contain samplers and channels");
				return false;
			}

			// Record the output shape required by each sampler's channels.
			std::vector<int> samplerComponents(samplerPositions.size(), 0);
			std::set<std::pair<int, AnimationPath>> targets;

			for (const int channelPosition : channelPositions) {
				AnimationChannel3D channel;

				if (!readRequiredInt(
					channelPosition, "sampler", channel._samplerIndex
				)) {
					return false;
				}

				if (static_cast<std::size_t>(channel._samplerIndex) >=
					samplerPositions.size()) {
					DebugError(L"Animation sampler index is out of range");
					return false;
				}

				if (!findObjectField(channelPosition, "target", true))
					return false;

				if (reader.peek() != '{') {
					DebugError(L"Expected an animation target object");
					return false;
				}

				const int targetPosition = reader._position;

				if (!readRequiredInt(targetPosition, "node", channel._nodeIndex)) {
					return false;
				}

				if (static_cast<std::size_t>(channel._nodeIndex) >= model._nodes.size()) {
					DebugError(L"Animation target node index is out of range");
					return false;
				}

				if (model._nodes[channel._nodeIndex]._hasMatrix) {
					DebugError(L"Animated node must use TRS instead of a matrix");
					return false;
				}

				if (!findObjectField(targetPosition, "path", true))
					return false;

				const std::string path = reader.readString();
				int components = 0;

				if (path == "translation") {
					channel._path = AnimationPath::Translation;
					components = 3;
				}
				else if (path == "rotation") {
					channel._path = AnimationPath::Rotation;
					components = 4;
				}
				else if (path == "scale") {
					channel._path = AnimationPath::Scale;
					components = 3;
				}
				else {
					DebugError(L"Unsupported animation target path");
					return false;
				}

				if (!targets.emplace(channel._nodeIndex, channel._path).second) {
					DebugError(L"Duplicate animation target");
					return false;
				}

				auto& expected = samplerComponents[channel._samplerIndex];

				if (expected != 0 && expected != components) {
					DebugError(L"Animation sampler is used with incompatible paths");
					return false;
				}

				expected = components;
				animation._channels.push_back(channel);
			}

			animation._samplers.reserve(samplerPositions.size());

			for (std::size_t i = 0; i < samplerPositions.size(); ++i) {
				const int samplerPosition = samplerPositions[i];
				AnimationSampler3D sampler;

				int inputAccessor = -1;
				int outputAccessor = -1;

				if (!readRequiredInt(samplerPosition, "input", inputAccessor) || !readRequiredInt(samplerPosition, "output", outputAccessor)) {
					return false;
				}

				if (findObjectField(samplerPosition, "interpolation", false)) {
					const std::string interpolation = reader.readString();

					if (interpolation == "LINEAR") {
						sampler._interpolation =
							AnimationInterpolation::Linear;
					}
					else if (interpolation == "STEP") {
						sampler._interpolation =
							AnimationInterpolation::Step;
					}
					else if (interpolation == "CUBICSPLINE") {
						sampler._interpolation = AnimationInterpolation::CubicSpline;
					}
					else {
						DebugError(L"Unsupported animation interpolation");
						return false;
					}
				}

				std::vector<std::array<float, 4>> times;

				if (!loadAnimationFloatAccessor(reader, binData, inputAccessor, 1, times, jsonIndex)) {
					return false;
				}

				sampler._times.reserve(times.size());

				for (std::size_t key = 0; key < times.size(); ++key) {
					const float time = times[key][0];

					if (time < 0.f ||
						(key > 0 && time <= times[key - 1][0])) {
						DebugError(L"Animation times must be non-negative and strictly increasing");
						return false;
					}

					sampler._times.push_back(time);
				}

				int components = samplerComponents[i];

				// Unused samplers still retain their array indices.
				if (components == 0) {
					if (!enterIndexedJsonObject(reader, jsonIndex._accessors, outputAccessor)) {
						return false;
					}

					if (!reader.findField("type"))
						return false;

					const std::string type = reader.readString();

					if (type == "SCALAR")
						components = 1;
					else if (type == "VEC3")
						components = 3;
					else if (type == "VEC4")
						components = 4;
					else {
						DebugError(L"Unsupported unused animation sampler type");
						return false;
					}
				}

				if (!loadAnimationFloatAccessor(reader, binData, outputAccessor, components, sampler._values, jsonIndex)) {
					return false;
				}

				const bool cubic = sampler._interpolation == AnimationInterpolation::CubicSpline;

				if (cubic && sampler._times.size() < 2) {
					DebugError(L"Cubic spline animation requires at least two keys");
					return false;
				}

				const std::uint64_t expectedValues = static_cast<std::uint64_t>(sampler._times.size()) * (cubic ? 3u : 1u);

				if (sampler._values.size() != expectedValues) {
					DebugError(L"Animation output count does not match input count");
					return false;
				}

				if (samplerComponents[i] != 0) {
					animation._duration = std::max(animation._duration, sampler._times.back());
				}

				animation._samplers.push_back(std::move(sampler));
			}

			// Validate quaternion values without modifying spline tangents.
			for (const auto& channel : animation._channels) {
				if (channel._path != AnimationPath::Rotation)
					continue;

				const auto& sampler = animation._samplers[channel._samplerIndex];

				const bool cubic = sampler._interpolation == AnimationInterpolation::CubicSpline;

				for (std::size_t key = 0; key < sampler._times.size(); ++key) {
					const auto& q = sampler._values[cubic ? key * 3 + 1 : key];

					const double lengthSquared =
						double(q[0]) * q[0] +
						double(q[1]) * q[1] +
						double(q[2]) * q[2] +
						double(q[3]) * q[3];

					if (std::abs(lengthSquared - 1.0) > 0.001) {
						DebugError(L"Animation rotation key is not a unit quaternion");
						return false;
					}
				}
			}

			animations.push_back(std::move(animation));
		}

		model._animations = std::move(animations);
		model._animationIndex = -1;
		model._animationTime = 0.0;

		DebugStat(L"Loaded animations: " + std::to_wstring(model._animations.size()));

		for (std::size_t i = 0; i < model._animations.size(); ++i) {
			const auto& animation = model._animations[i];

			DebugStat(
				L"Animation " + std::to_wstring(i) +
				L": duration = " + std::to_wstring(animation._duration) +
				L" s, channels = " +
				std::to_wstring(animation._channels.size()) +
				L", samplers = " +
				std::to_wstring(animation._samplers.size())
			);
		}

		return true;
	}

	std::shared_ptr<Model3D> openModel3DFromFile(std::filesystem::path path) {

		DebugLog(L"Load the model \"" + path.wstring() + L"\"");

		std::ifstream file;
		file.open(path, std::ios::in | std::ios::binary);
		if (!file) {
			DebugError(L"Nie udało się otworzyć modelu");
			return nullptr;
		}
		
		// .. loading ..

		BinaryReader reader(file);

		uint32_t magic = reader.read_uint32();
		uint32_t version = reader.read_uint32();
		uint32_t length = reader.read_uint32();

		if (magic != 0x46546C67) {
			DebugError(L"The file is not a GLB file.");
			return nullptr;
		}

		if (version != 2) {
			DebugError(L"Unsupported GLB version");
			return nullptr;
		}

		DebugStat(L"Version: " + std::to_wstring(version));
		DebugStat(L"Size: " + std::to_wstring(length) + L" bytes");

		uint32_t chunkLength = reader.read_uint32();
		uint32_t chunkType = reader.read_uint32();

		if (chunkType != 0x4E4F534A) { // "JSON"
			DebugError(L"The first GLB chunk is not JSON");
			return nullptr;
		}

		if (length < 20 || chunkLength > length - 20) {
			DebugError(L"Invalid JSON chunk size");
			return nullptr;
		}

		std::string jsonText(chunkLength, '\0');

		if (!file.read(jsonText.data(), chunkLength)) {
			DebugError(L"Failed to read the JSON chunk");
			return nullptr;
		}

		DebugStat(L"JSON size: " + std::to_wstring(chunkLength));

		uint32_t binLength = reader.read_uint32();
		uint32_t binType = reader.read_uint32();

		if (binType != 0x004E4942) { // "BIN"
			DebugError(L"Expected a BIN chunk.");
			return nullptr;
		}

		// Nagłówek GLB + nagłówek JSON + JSON + nagłówek BIN.
		const uint64_t binStart = 12ull + 8ull + chunkLength + 8ull;

		if (binStart > length || binLength > length - binStart) {
			DebugError(L"Invalid BIN chunk size");
			return nullptr;
		}

		std::vector<uint8_t> binData(binLength);

		if (!file.read(reinterpret_cast<char*>(binData.data()), binLength)) {
			DebugError(L"Failed to read the BIN chunk");
			return nullptr;
		}

		DebugStat(L"BIN size: " + std::to_wstring(binLength));
		
		// Json reading

		JsonReader jsonReader(jsonText);

		if (!jsonReader.findRootField("meshes"))
			return nullptr;

		if (!jsonReader.expect('['))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("primitives"))
			return nullptr;

		if (!jsonReader.expect('['))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("attributes"))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("POSITION"))
			return nullptr;

		int positionAccessor = 0;

		if (!jsonReader.readInt(positionAccessor))
			return nullptr;

		DebugStat(L"Accessor position: " + std::to_wstring(positionAccessor));

		if (!jsonReader.findRootField("accessors"))
			return nullptr;

		if (!jsonReader.enterArrayElement(positionAccessor))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int positionAccessorStart = jsonReader._position;

		if (!jsonReader.findField("count"))
			return nullptr;

		int vertexCount = 0;

		if (!jsonReader.readInt(vertexCount))
			return nullptr;

		DebugStat(L"Vertices count: " + std::to_wstring(vertexCount));
		
		jsonReader._position = positionAccessorStart;

		if (!jsonReader.findField("bufferView"))
			return nullptr;

		int positionBufferView = 0;

		if (!jsonReader.readInt(positionBufferView))
			return nullptr;

		DebugStat(L"BufferView position: " + std::to_wstring(positionBufferView));

		if (!jsonReader.findRootField("bufferViews"))
			return nullptr;

		if (!jsonReader.enterArrayElement(positionBufferView))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int positionBufferViewStart = jsonReader._position;

		if (!jsonReader.findField("byteOffset"))
			return nullptr;

		int positionBufferOffset = 0;

		if (!jsonReader.readInt(positionBufferOffset))
			return nullptr;

		DebugStat(L"Position buffer offset: " + std::to_wstring(positionBufferOffset));

		jsonReader._position = positionAccessorStart;

		if (!jsonReader.findField("componentType"))
			return nullptr;

		int positionComponentType = 0;

		if (!jsonReader.readInt(positionComponentType))
			return nullptr;

		if (positionComponentType != 5126) { // float
			DebugError(L"Positions are not stored as floats");
			return nullptr;
		}

		jsonReader._position = positionAccessorStart;

		if (!jsonReader.findField("type"))
			return nullptr;

		std::string positionType = jsonReader.readString();

		if (positionType != "VEC3") {
			DebugError(L"Positions are not stored as VEC3");
			return nullptr;
		}

		DebugStat(L"Position format: float, VEC3");

		jsonReader._position = positionBufferViewStart;

		int positionStride = 12;

		if (jsonReader.findField("byteStride", false)) {
			if (!jsonReader.readInt(positionStride))
				return nullptr;
		}

		if (positionStride < 12 || positionStride > 252 || positionStride % 4 != 0) {
			DebugError(L"Invalid position byte stride");
			return nullptr;
		}

		DebugStat(L"Position byte stride: " + std::to_wstring(positionStride));

		jsonReader._position = positionAccessorStart;

		int positionAccessorOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(positionAccessorOffset))
				return nullptr;
		}

		DebugStat(L"Position accessor offset: " + std::to_wstring(positionAccessorOffset));

		// Load The Mesh
		const uint64_t positionsStart = static_cast<uint64_t>(positionBufferOffset) + static_cast<uint64_t>(positionAccessorOffset);

		if (vertexCount <= 0) {
			DebugError(L"Invalid vertex count");
			return nullptr;
		}

		const uint64_t positionsEnd = positionsStart + static_cast<uint64_t>(vertexCount - 1) * positionStride + 12;

		if (positionsEnd > binData.size()) {
			DebugError(L"Position data exceeds BIN bounds");
			return nullptr;
		}

		std::shared_ptr<Mesh3D> mesh = std::make_shared<Mesh3D>();
		mesh->_vertices.reserve(vertexCount);

		for (int i = 0; i < vertexCount; ++i) {
			const uint64_t offset = positionsStart + static_cast<uint64_t>(i) * positionStride;

			float position[3];

			std::memcpy(
				position,
				binData.data() + offset,
				sizeof(position)
			);

			mesh->_vertices.emplace_back(
				position[0], position[1], position[2],
				0.f, 0.f, 0.f, // normals — later
				0.f, 0.f       // UV — later
			);
		}

		DebugStat(L"Loaded vertices: " + std::to_wstring(mesh->_vertices.size()));

		if (!jsonReader.findRootField("meshes"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("primitives"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("indices"))
			return nullptr;

		int indicesAccessor = 0;

		if (!jsonReader.readInt(indicesAccessor))
			return nullptr;

		DebugStat(L"Indices accessor: " + std::to_wstring(indicesAccessor));

		if (!jsonReader.findRootField("accessors"))
			return nullptr;

		if (!jsonReader.enterArrayElement(indicesAccessor))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int indicesAccessorStart = jsonReader._position;

		if (!jsonReader.findField("count"))
			return nullptr;

		int indicesCount = 0;

		if (!jsonReader.readInt(indicesCount))
			return nullptr;

		DebugStat(L"Indices count: " + std::to_wstring(indicesCount));

		jsonReader._position = indicesAccessorStart;

		if (!jsonReader.findField("componentType"))
			return nullptr;

		int indicesComponentType = 0;

		if (!jsonReader.readInt(indicesComponentType))
			return nullptr;

		int indexSize = 0;

		if (indicesComponentType == 5121) {
			indexSize = 1;
		}
		else if (indicesComponentType == 5123) {
			indexSize = 2;
		}
		else if (indicesComponentType == 5125) {
			indexSize = 4;
		}
		else {
			DebugError(L"Unsupported indices component type");
			return nullptr;
		}

		jsonReader._position = indicesAccessorStart;

		if (!jsonReader.findField("type"))
			return nullptr;

		if (jsonReader.readString() != "SCALAR") {
			DebugError(L"Indices are not stored as SCALAR");
			return nullptr;
		}

		jsonReader._position = indicesAccessorStart;

		if (!jsonReader.findField("bufferView"))
			return nullptr;

		int indicesBufferView = 0;

		if (!jsonReader.readInt(indicesBufferView))
			return nullptr;

		DebugStat(L"Indices format: uint32, SCALAR");
		DebugStat(L"Indices buffer view: " + std::to_wstring(indicesBufferView));

		// Optional offset inside the buffer view.
		jsonReader._position = indicesAccessorStart;

		int indicesAccessorOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(indicesAccessorOffset))
				return nullptr;
		}

		// Find the buffer view.
		if (!jsonReader.findRootField("bufferViews"))
			return nullptr;

		if (!jsonReader.enterArrayElement(indicesBufferView))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int indicesBufferViewStart = jsonReader._position;

		int indicesBufferOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(indicesBufferOffset))
				return nullptr;
		}

		DebugStat(L"Indices accessor offset: " + std::to_wstring(indicesAccessorOffset));
		DebugStat(L"Indices buffer offset: " + std::to_wstring(indicesBufferOffset));

		const uint64_t indicesStart = static_cast<uint64_t>(indicesBufferOffset) + static_cast<uint64_t>(indicesAccessorOffset);

		if (indicesCount <= 0 || indicesCount % 3 != 0) {
			DebugError(L"Invalid index count for triangles");
			return nullptr;
		}

		const uint64_t indicesBytes = static_cast<uint64_t>(indicesCount) * indexSize;

		if (indicesStart > binData.size() ||
			indicesBytes > binData.size() - indicesStart) {
			DebugError(L"Index data exceeds BIN bounds");
			return nullptr;
		}

		mesh->_indices.resize(indicesCount);

		for (int i = 0; i < indicesCount; ++i) {
			const uint64_t offset = indicesStart + static_cast<uint64_t>(i) * indexSize;

			if (indicesComponentType == 5121) {
				mesh->_indices[i] = binData[offset];
			}
			else if (indicesComponentType == 5123) {
				uint16_t index = 0;
				std::memcpy(&index, binData.data() + offset, sizeof(index));
				mesh->_indices[i] = index;
			}
			else {
				uint32_t index = 0;
				std::memcpy(&index, binData.data() + offset, sizeof(index));
				mesh->_indices[i] = index;
			}
		}

		for (const auto index : mesh->_indices) {
			if (index >= mesh->_vertices.size()) {
				DebugError(L"Index exceeds vertex count");
				return nullptr;
			}
		}

		DebugStat(L"Loaded indices: " + std::to_wstring(mesh->_indices.size()));

		if (!jsonReader.findRootField("meshes"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("primitives"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("attributes"))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("NORMAL"))
			return nullptr;

		int normalAccessor = 0;

		if (!jsonReader.readInt(normalAccessor))
			return nullptr;

		DebugStat(L"Normal accessor: " + std::to_wstring(normalAccessor));

		if (!jsonReader.findRootField("accessors"))
			return nullptr;

		if (!jsonReader.enterArrayElement(normalAccessor))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int normalAccessorStart = jsonReader._position;

		if (!jsonReader.findField("count"))
			return nullptr;

		int normalCount = 0;

		if (!jsonReader.readInt(normalCount))
			return nullptr;

		if (normalCount != vertexCount) {
			DebugError(L"Normal count does not match vertex count");
			return nullptr;
		}

		jsonReader._position = normalAccessorStart;

		if (!jsonReader.findField("bufferView"))
			return nullptr;

		int normalBufferView = 0;

		if (!jsonReader.readInt(normalBufferView))
			return nullptr;

		DebugStat(L"Normal count: " + std::to_wstring(normalCount));
		DebugStat(L"Normal buffer view: " + std::to_wstring(normalBufferView));

		jsonReader._position = normalAccessorStart;

		if (!jsonReader.findField("componentType"))
			return nullptr;

		int normalComponentType = 0;

		if (!jsonReader.readInt(normalComponentType))
			return nullptr;

		if (normalComponentType != 5126) {
			DebugError(L"Normals are not stored as floats");
			return nullptr;
		}

		jsonReader._position = normalAccessorStart;

		if (!jsonReader.findField("type"))
			return nullptr;

		if (jsonReader.readString() != "VEC3") {
			DebugError(L"Normals are not stored as VEC3");
			return nullptr;
		}

		jsonReader._position = normalAccessorStart;

		int normalAccessorOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(normalAccessorOffset))
				return nullptr;
		}

		DebugStat(L"Normal format: float, VEC3");
		DebugStat(L"Normal accessor offset: " + std::to_wstring(normalAccessorOffset));

		if (!jsonReader.findRootField("bufferViews"))
			return nullptr;

		if (!jsonReader.enterArrayElement(normalBufferView))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int normalBufferViewStart = jsonReader._position;

		int normalBufferOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(normalBufferOffset))
				return nullptr;
		}

		jsonReader._position = normalBufferViewStart;

		int normalStride = 12; // Default for float, VEC3.

		if (jsonReader.findField("byteStride", false)) {
			if (!jsonReader.readInt(normalStride))
				return nullptr;
		}

		if (normalStride < 12 || normalStride > 252 || normalStride % 4 != 0) {
			DebugError(L"Invalid normal byte stride");
			return nullptr;
		}

		DebugStat(L"Normal buffer offset: " + std::to_wstring(normalBufferOffset));
		DebugStat(L"Normal byte stride: " + std::to_wstring(normalStride));

		const uint64_t normalsStart = static_cast<uint64_t>(normalBufferOffset) + static_cast<uint64_t>(normalAccessorOffset);

		if (normalCount <= 0 || static_cast<std::size_t>(normalCount) != mesh->_vertices.size()) {
			DebugError(L"Invalid normal count");
			return nullptr;
		}

		const uint64_t normalsEnd = normalsStart + static_cast<uint64_t>(normalCount - 1) * normalStride + 12;

		if (normalsEnd > binData.size()) {
			DebugError(L"Normal data exceeds BIN bounds");
			return nullptr;
		}

		for (int i = 0; i < normalCount; ++i) {
			const uint64_t offset = normalsStart + static_cast<uint64_t>(i) * normalStride;

			float normal[3];

			std::memcpy(
				normal,
				binData.data() + offset,
				sizeof(normal)
			);

			auto& vertex = mesh->_vertices[i];

			vertex._nx = normal[0];
			vertex._ny = normal[1];
			vertex._nz = normal[2];
		}

		DebugStat(L"Loaded normals: " + std::to_wstring(normalCount));

		if (!jsonReader.findRootField("meshes"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("primitives"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("attributes"))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("TEXCOORD_0"))
			return nullptr;

		int uvAccessor = 0;

		if (!jsonReader.readInt(uvAccessor))
			return nullptr;

		DebugStat(L"UV accessor: " + std::to_wstring(uvAccessor));

		if (!jsonReader.findRootField("accessors"))
			return nullptr;

		if (!jsonReader.enterArrayElement(uvAccessor))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int uvAccessorStart = jsonReader._position;

		if (!jsonReader.findField("count"))
			return nullptr;

		int uvCount = 0;

		if (!jsonReader.readInt(uvCount))
			return nullptr;

		if (uvCount != vertexCount) {
			DebugError(L"UV count does not match vertex count");
			return nullptr;
		}

		jsonReader._position = uvAccessorStart;

		if (!jsonReader.findField("bufferView"))
			return nullptr;

		int uvBufferView = 0;

		if (!jsonReader.readInt(uvBufferView))
			return nullptr;

		DebugStat(L"UV count: " + std::to_wstring(uvCount));
		DebugStat(L"UV buffer view: " + std::to_wstring(uvBufferView));

		jsonReader._position = uvAccessorStart;

		if (!jsonReader.findField("componentType"))
			return nullptr;

		int uvComponentType = 0;

		if (!jsonReader.readInt(uvComponentType))
			return nullptr;

		if (uvComponentType != 5126) {
			DebugError(L"Only float UV coordinates are currently supported");
			return nullptr;
		}

		jsonReader._position = uvAccessorStart;

		if (!jsonReader.findField("type"))
			return nullptr;

		if (jsonReader.readString() != "VEC2") {
			DebugError(L"UV coordinates are not stored as VEC2");
			return nullptr;
		}

		jsonReader._position = uvAccessorStart;

		int uvAccessorOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(uvAccessorOffset))
				return nullptr;
		}

		DebugStat(L"UV format: float, VEC2");
		DebugStat(L"UV accessor offset: " + std::to_wstring(uvAccessorOffset));

		if (!jsonReader.findRootField("bufferViews"))
			return nullptr;

		if (!jsonReader.enterArrayElement(uvBufferView))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int uvBufferViewStart = jsonReader._position;

		int uvBufferOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(uvBufferOffset))
				return nullptr;
		}

		jsonReader._position = uvBufferViewStart;

		int uvStride = 8; // Two floats: u, v.

		if (jsonReader.findField("byteStride", false)) {
			if (!jsonReader.readInt(uvStride))
				return nullptr;
		}

		if (uvStride < 8 || uvStride > 252 || uvStride % 4 != 0) {
			DebugError(L"Invalid UV byte stride");
			return nullptr;
		}

		DebugStat(L"UV buffer offset: " + std::to_wstring(uvBufferOffset));
		DebugStat(L"UV byte stride: " + std::to_wstring(uvStride));

		const uint64_t uvStart = static_cast<uint64_t>(uvBufferOffset) + static_cast<uint64_t>(uvAccessorOffset);

		if (uvCount <= 0 ||
			static_cast<std::size_t>(uvCount) != mesh->_vertices.size()) {
			DebugError(L"Invalid UV count");
			return nullptr;
		}

		const uint64_t uvEnd = uvStart + static_cast<uint64_t>(uvCount - 1) * uvStride + 8;

		if (uvEnd > binData.size()) {
			DebugError(L"UV data exceeds BIN bounds");
			return nullptr;
		}

		for (int i = 0; i < uvCount; ++i) {
			const uint64_t offset = uvStart + static_cast<uint64_t>(i) * uvStride;

			float uv[2];

			std::memcpy(
				uv,
				binData.data() + offset,
				sizeof(uv)
			);

			auto& vertex = mesh->_vertices[i];

			vertex._u = uv[0];
			vertex._v = uv[1];
		}

		DebugStat(L"Loaded UV coordinates: " + std::to_wstring(uvCount));

		if (!jsonReader.findRootField("meshes"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("primitives"))
			return nullptr;

		if (!jsonReader.enterArrayElement(0))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("material"))
			return nullptr;

		int materialIndex = 0;

		if (!jsonReader.readInt(materialIndex))
			return nullptr;

		DebugStat(L"Material index: " + std::to_wstring(materialIndex));

		if (!jsonReader.findRootField("materials"))
			return nullptr;

		if (!jsonReader.enterArrayElement(materialIndex))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("pbrMetallicRoughness"))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("baseColorTexture"))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("index"))
			return nullptr;

		int baseColorTextureIndex = 0;

		if (!jsonReader.readInt(baseColorTextureIndex))
			return nullptr;

		DebugStat(L"Base color texture index: " + std::to_wstring(baseColorTextureIndex));

		if (!jsonReader.findRootField("textures"))
			return nullptr;

		if (!jsonReader.enterArrayElement(baseColorTextureIndex))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		if (!jsonReader.findField("source"))
			return nullptr;

		int baseColorImageIndex = 0;

		if (!jsonReader.readInt(baseColorImageIndex))
			return nullptr;

		DebugStat(L"Base color image index: " + std::to_wstring(baseColorImageIndex));

		if (!jsonReader.findRootField("images"))
			return nullptr;

		if (!jsonReader.enterArrayElement(baseColorImageIndex))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int baseColorImageStart = jsonReader._position;

		if (!jsonReader.findField("mimeType"))
			return nullptr;

		std::string imageMimeType = jsonReader.readString();

		if (imageMimeType != "image/png" && imageMimeType != "image/jpeg") {
			DebugError(L"Only PNG and JPEG images are currently supported");
			return nullptr;
		}

		jsonReader._position = baseColorImageStart;

		if (!jsonReader.findField("bufferView"))
			return nullptr;

		int baseColorImageBufferView = 0;

		if (!jsonReader.readInt(baseColorImageBufferView))
			return nullptr;

		DebugStat(L"Base color image format: PNG");
		DebugStat(L"Base color image buffer view: " + std::to_wstring(baseColorImageBufferView));

		if (!jsonReader.findRootField("bufferViews"))
			return nullptr;

		if (!jsonReader.enterArrayElement(baseColorImageBufferView))
			return nullptr;

		if (!jsonReader.expect('{'))
			return nullptr;

		int imageBufferViewStart = jsonReader._position;

		int imageBufferOffset = 0;

		if (jsonReader.findField("byteOffset", false)) {
			if (!jsonReader.readInt(imageBufferOffset))
				return nullptr;
		}

		jsonReader._position = imageBufferViewStart;

		if (!jsonReader.findField("byteLength"))
			return nullptr;

		int imageByteLength = 0;

		if (!jsonReader.readInt(imageByteLength))
			return nullptr;

		if (imageByteLength <= 0 ||
			static_cast<uint64_t>(imageBufferOffset) > binData.size() ||
			static_cast<uint64_t>(imageByteLength) >
			binData.size() - static_cast<uint64_t>(imageBufferOffset)) {
			DebugError(L"Image data exceeds BIN bounds");
			return nullptr;
		}

		DebugStat(L"Image buffer offset: " + std::to_wstring(imageBufferOffset));
		DebugStat(L"Image byte length: " + std::to_wstring(imageByteLength));

		auto image = std::make_shared<sf::Image>();

		if (!image->loadFromMemory(
			binData.data() + imageBufferOffset,
			static_cast<std::size_t>(imageByteLength))) {
			DebugError(L"Failed to decode base color image");
			return nullptr;
		}

		auto texture = std::make_shared<Texture>(
			path.wstring() + L"#baseColor",
			image
		);

		if (texture->_texture->getSize().x == 0 ||
			texture->_texture->getSize().y == 0) {
			DebugError(L"Failed to create base color texture");
			return nullptr;
		}

		mesh->setTexture(texture);

		DebugStat(L"Base color texture loaded");

		file.close();

		std::shared_ptr<Model3D> model = std::make_shared<Model3D>();

		if (!loadModelNodes(jsonReader, *model))
			return nullptr;

		if (!loadModelSkins(jsonReader, binData, *model))
			return nullptr;

		if (!loadMeshSkinData(jsonReader, binData, *model, *mesh))
			return nullptr;

		DebugStat(L"Loading model animations...");

		if (!loadModelAnimations(jsonReader, binData, *model)) {
			DebugError(L"Failed to load model animations");
			return nullptr;
		}

		DebugStat(L"Finished loading model animations");

		mesh->uploadToGPU();

		if (!uploadMeshSkinPose(*model, *mesh))
			return nullptr;

		DebugStat(L"Uploaded joint matrices: " + std::to_wstring(mesh->_gpuJointCount));

		model->addMesh(mesh);

		if (!model->_animations.empty()) {
			if (!model->playAnimation(25, true)) {
				DebugError(L"Failed to start model animation");
				return nullptr;
			}
		}

		return model;

	}
}
