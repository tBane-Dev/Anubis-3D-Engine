#pragma once
#include <vector>
#include "3D/Quaternion.hpp"
#include "3D/Vertex3D.hpp"
#include "3D/Mesh3D.hpp"
#include "3D/Skeleton3D.hpp"
#include "3D/Animation3D.hpp"
#include "TexturesManager.hpp"
#include "JsonReader.hpp"

namespace Anubis {
	class Model3D : public std::enable_shared_from_this<Model3D> {
	public:
		static int _nextID;
		int _id;
		float _x, _y, _z;
		Quaternion _rotation;
		float _scale;
		bool _isSelectable;

		Quaternion _rotationDelta;
		float _rotationSpeed;

		std::vector<std::shared_ptr<Mesh3D>> _meshes;

		std::vector<Node3D> _nodes;
		std::vector<Skin3D> _skins;

		std::vector<Animation3D> _animations;
		int _animationIndex = -1;	// A negative index disables animation playback.
		double _animationTime = 0.0;
		float _animationSpeed = 1.f;
		bool _animationLoop = true;

		sf::Vector2f offset;
		sf::Vector2f _lastRotationMouse;

		Model3D(float x = 0, float y = 0, float z = 0, Quaternion rotation = Quaternion(1.0f, 0.0f, 0.0f, 0.0f), float scale = 1.0f);
		~Model3D();

		void addMesh(std::shared_ptr<Anubis::Mesh3D> mesh);

		bool playAnimation(int index, bool loop = true);
		bool updateAnimation(float deltaSeconds);
		bool applyAnimationPose();

		void handleEvent(const sf::Event& event);
		void update();
		void drawShape();
		void draw();
	};

	static bool loadModelNodes(JsonReader& reader, Model3D& model);
	static bool loadInverseBindMatrices(JsonReader& reader, const std::vector<std::uint8_t>& binData, int accessorIndex, Skin3D& skin);
	static bool loadModelSkins(JsonReader& reader, const std::vector<std::uint8_t>& binData, Model3D& model);
	static bool loadSkinAttribute(JsonReader& reader, const std::vector<std::uint8_t>& binData, int accessorIndex, std::size_t vertexCount, bool joints, std::vector<std::array<float, 4>>& values);
	static bool loadMeshSkinData(JsonReader& reader, const std::vector<std::uint8_t>& binData, const Model3D& model, Mesh3D& mesh);
	static bool uploadMeshSkinPose(const Model3D& model, Mesh3D& mesh);
	static bool sampleAnimationChannel(const AnimationSampler3D& sampler, AnimationPath path, double time, std::array<float, 4>& result);

	struct AnimationJsonIndex {
		std::vector<int> _accessors;
		std::vector<int> _bufferViews;
	};
	static bool indexJsonObjectArray(JsonReader& reader, const std::string& field, std::vector<int>& positions);
	static bool enterIndexedJsonObject(JsonReader& reader, const std::vector<int>& positions, int index);
	static bool loadAnimationFloatAccessor(JsonReader& reader, const std::vector<std::uint8_t>& binData, int accessorIndex, int expectedComponents, std::vector<std::array<float, 4>>& values, const AnimationJsonIndex& jsonIndex);	
	static bool loadModelAnimations(JsonReader& reader, const std::vector<std::uint8_t>& binData, Model3D& model);

	std::shared_ptr<Model3D> openModel3DFromFile(std::filesystem::path path);
}