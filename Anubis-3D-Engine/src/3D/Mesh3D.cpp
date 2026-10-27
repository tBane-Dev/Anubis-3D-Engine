#include "3D/OpenGLBuffers.hpp"
#include "3D/Mesh3D.hpp"
#include "3D/Scene.hpp"
#include <SFML/OpenGL.hpp>
#include "RenderWindow.hpp"
#include <iostream>
#include "Cursor.hpp"
#include "DebugLog.hpp"

namespace Anubis {
	Mesh3D::Mesh3D() {

	}

	Mesh3D::~Mesh3D() {
		if (_vertexArray != 0)
			GL::deleteVertexArrays(1, &_vertexArray);

		if (_vertexBuffer != 0)
			GL::deleteBuffers(1, &_vertexBuffer);

		if (_indexBuffer != 0)
			GL::deleteBuffers(1, &_indexBuffer);

		if (_jointTexture != 0)
			glDeleteTextures(1, &_jointTexture);

		if (_jointBuffer != 0)
			GL::deleteBuffers(1, &_jointBuffer);
	}

	void Mesh3D::setTexture(std::shared_ptr<Texture> texture) {
		_texture = texture;
	}

	bool Mesh3D::uploadJointMatrices(const std::vector<Matrix4>& matrices) {
		static_assert(sizeof(Matrix4) == 16 * sizeof(float));

		if (!GL::genBuffers || !GL::bindBuffer || !GL::bufferData || !GL::texBuffer) {
			DebugError(L"OpenGL joint buffer functions are not initialized");
			return false;
		}

		if (matrices.empty()) {
			DebugError(L"Joint matrix array is empty");
			return false;
		}

		GLint maxTexels = 0;
		glGetIntegerv(GL::MaxTextureBufferSize, &maxTexels);

		// Each matrix occupies four RGBA32F texels.
		if (maxTexels <= 0 || matrices.size() > static_cast<std::size_t>(maxTexels) / 4) {
			DebugError(L"Joint matrices exceed the GPU texture buffer limit");
			return false;
		}

		const auto maxBytes = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());

		if (matrices.size() > maxBytes / sizeof(Matrix4)) {
			DebugError(L"Joint matrix buffer is too large");
			return false;
		}

		for (const auto& matrix : matrices) {
			for (const float value : matrix) {
				if (!std::isfinite(value)) {
					DebugError(L"Joint matrix contains a non-finite value");
					return false;
				}
			}
		}

		GLint previousBuffer = 0;
		GLint previousTexture = 0;

		glGetIntegerv(GL::TextureBuffer, &previousBuffer);
		glGetIntegerv(GL::TextureBindingBuffer, &previousTexture);

		if (_jointBuffer == 0)
			GL::genBuffers(1, &_jointBuffer);

		if (_jointTexture == 0)
			glGenTextures(1, &_jointTexture);

		if (_jointBuffer == 0 || _jointTexture == 0) {
			DebugError(L"Failed to create joint buffer objects");
			return false;
		}

		GL::bindBuffer(GL::TextureBuffer, _jointBuffer);

		GL::bufferData(
			GL::TextureBuffer,
			static_cast<std::ptrdiff_t>(
				matrices.size() * sizeof(Matrix4)
				),
			matrices.data(),
			GL::DynamicDraw
		);

		glBindTexture(GL::TextureBuffer, _jointTexture);
		GL::texBuffer(GL::TextureBuffer, GL::RGBA32F, _jointBuffer);

		// Restore the previous buffer and texture bindings.
		glBindTexture(GL::TextureBuffer, static_cast<GLuint>(previousTexture));

		GL::bindBuffer(GL::TextureBuffer, static_cast<GLuint>(previousBuffer));

		_gpuJointCount = matrices.size();
		return true;
	}

	void Mesh3D::uploadToGPU() {
		static_assert(std::is_standard_layout_v<Vertex3D>);

		if (!GL::genBuffers || !GL::bindBuffer || !GL::bufferData ||
			!GL::genVertexArrays || !GL::bindVertexArray ||
			!GL::enableVertexAttribArray ||
			!GL::vertexAttribPointer || !GL::vertexAttribIPointer) {
			DebugError(L"OpenGL vertex buffer functions are not initialized");
			return;
		}

		if (_indices.size() >
			static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
			DebugError(L"Too many indices");
			return;
		}

		const auto maxBufferBytes = static_cast<std::size_t>(
			std::numeric_limits<std::ptrdiff_t>::max()
			);

		if (_vertices.size() > maxBufferBytes / sizeof(Vertex3D) ||
			_indices.size() > maxBufferBytes / sizeof(std::uint32_t)) {
			DebugError(L"Mesh buffer is too large");
			return;
		}

		for (const auto index : _indices) {
			if (index >= _vertices.size()) {
				DebugError(L"Mesh index exceeds vertex count");
				return;
			}
		}

		// Zachowujemy stan zastany przed przesłaniem danych.
		constexpr GLenum vertexArrayBinding = 0x85B5;
		constexpr GLenum arrayBufferBinding = 0x8894;

		GLint previousVertexArray = 0;
		GLint previousArrayBuffer = 0;

		glGetIntegerv(vertexArrayBinding, &previousVertexArray);
		glGetIntegerv(arrayBufferBinding, &previousArrayBuffer);

		if (_vertexArray == 0)
			GL::genVertexArrays(1, &_vertexArray);

		if (_vertexBuffer == 0)
			GL::genBuffers(1, &_vertexBuffer);

		if (_indexBuffer == 0)
			GL::genBuffers(1, &_indexBuffer);

		GL::bindVertexArray(_vertexArray);

		GL::bindBuffer(GL::ArrayBuffer, _vertexBuffer);

		GL::bufferData(
			GL::ArrayBuffer,
			static_cast<std::ptrdiff_t>(
				_vertices.size() * sizeof(Vertex3D)
				),
			_vertices.data(),
			GL::StaticDraw
		);

		GL::bindBuffer(GL::ElementArrayBuffer, _indexBuffer);

		GL::bufferData(
			GL::ElementArrayBuffer,
			static_cast<std::ptrdiff_t>(
				_indices.size() * sizeof(std::uint32_t)
				),
			_indices.data(),
			GL::StaticDraw
		);

		const auto stride = static_cast<GLsizei>(sizeof(Vertex3D));

		// 0 - positions
		GL::enableVertexAttribArray(0);
		GL::vertexAttribPointer(
			0, 3, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<const void*>(offsetof(Vertex3D, _x))
		);

		// 1 - normals
		GL::enableVertexAttribArray(1);
		GL::vertexAttribPointer(
			1, 3, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<const void*>(offsetof(Vertex3D, _nx))
		);

		// 2 - uv tex coords
		GL::enableVertexAttribArray(2);
		GL::vertexAttribPointer(
			2, 2, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<const void*>(offsetof(Vertex3D, _u))
		);

		// 3 - bones indices
		GL::enableVertexAttribArray(3);
		GL::vertexAttribIPointer(
			3, 4, GL_UNSIGNED_SHORT, stride,
			reinterpret_cast<const void*>(offsetof(Vertex3D, _joints))
		);

		// 4 - bones weights
		GL::enableVertexAttribArray(4);
		GL::vertexAttribPointer(
			4, 4, GL_FLOAT, GL_FALSE, stride,
			reinterpret_cast<const void*>(offsetof(Vertex3D, _weights))
		);

		// Restore the previous VAO and vertex buffer.
		GL::bindVertexArray(static_cast<GLuint>(previousVertexArray));
		GL::bindBuffer(
			GL::ArrayBuffer,
			static_cast<GLuint>(previousArrayBuffer)
		);

		_gpuIndexCount = static_cast<GLsizei>(_indices.size());
	}

	void Mesh3D::drawBuffers(bool textured) {
		if (_vertexArray == 0 || _gpuIndexCount == 0)
			return;

		if (!scene || scene->_modelShader.getNativeHandle() == 0) {
			DebugError(L"Model3D shader is not loaded");
			return;
		}

		if (!GL::bindVertexArray || !GL::activeTexture || !GL::useProgram) {
			DebugError(L"OpenGL drawing functions are not initialized");
			return;
		}

		auto& shader = scene->_modelShader;

		// Read the matrices prepared by Scene and Mesh3D.
		GLfloat modelView[16];
		GLfloat projection[16];
		GLfloat color[4];

		glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
		glGetFloatv(GL_PROJECTION_MATRIX, projection);
		glGetFloatv(GL_CURRENT_COLOR, color);

		const GLfloat identity[16] = {
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};

		// Preserve the state changed by this draw call.
		constexpr GLenum currentProgram = 0x8B8D;
		constexpr GLenum vertexArrayBinding = 0x85B5;
		constexpr GLenum activeTextureBinding = 0x84E0;
		constexpr GLenum textureUnit0 = 0x84C0;
		constexpr GLenum textureUnit1 = 0x84C1;

		GLint previousProgram = 0;
		GLint previousVertexArray = 0;
		GLint previousActiveTexture = 0;
		GLint previousTexture = 0;
		GLint previousJointTexture = 0;

		// Save the original state before changing texture units.
		glGetIntegerv(currentProgram, &previousProgram);
		glGetIntegerv(vertexArrayBinding, &previousVertexArray);
		glGetIntegerv(activeTextureBinding, &previousActiveTexture);

		GL::activeTexture(textureUnit1);
		glGetIntegerv(GL::TextureBindingBuffer, &previousJointTexture);

		GL::activeTexture(textureUnit0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

		const bool hasTexture = textured && _texture && _texture->_texture;

		// Model transformation is already included in modelView.
		shader.setUniform("uModel", sf::Glsl::Mat4(identity));
		shader.setUniform("uView", sf::Glsl::Mat4(modelView));
		shader.setUniform("uProjection", sf::Glsl::Mat4(projection));

		// Skeleton loading will enable skinning later.
		const bool skinned = _jointTexture != 0 && _gpuJointCount > 0;
		shader.setUniform("uSkinned", skinned);
		shader.setUniform("uPicking", !textured);
		shader.setUniform("uTextured", hasTexture);

		shader.setUniform("uTint", sf::Glsl::Vec4(color[0], color[1], color[2], color[3]));
		shader.setUniform("uPickColor", sf::Glsl::Vec3(color[0], color[1], color[2]));

		sf::Shader::bind(&shader);

		// Bind joint matrices to the unit used by uJointMatrices.
		GL::activeTexture(textureUnit1);
		glBindTexture(GL::TextureBuffer, skinned ? _jointTexture : 0);

		// Bind the model texture to unit zero.
		GL::activeTexture(textureUnit0);

		glBindTexture(GL_TEXTURE_2D, hasTexture ? _texture->_texture->getNativeHandle() : 0);

		GL::bindVertexArray(_vertexArray);

		glDrawElements(GL_TRIANGLES, _gpuIndexCount, GL_UNSIGNED_INT, nullptr);

		const GLenum error = glGetError();

		if (error != GL_NO_ERROR) {
			DebugError(
				L"OpenGL error after drawing mesh: " +
				std::to_wstring(static_cast<unsigned int>(error))
			);
		}

		// Restore the previous OpenGL state.
		GL::bindVertexArray(static_cast<GLuint>(previousVertexArray));

		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

		GL::activeTexture(textureUnit1);

		glBindTexture(GL::TextureBuffer, static_cast<GLuint>(previousJointTexture));

		GL::activeTexture(static_cast<GLenum>(previousActiveTexture));
		GL::useProgram(static_cast<GLuint>(previousProgram));
	}

	void Mesh3D::drawShape(float x, float y, float z, Anubis::Quaternion rotation, float scale) {
		glPushMatrix();
		glTranslatef(x, y, z);
		const auto matrix = rotation.matrix();
		glMultMatrixf(matrix.data());
		glScalef(scale, scale, scale);

		drawBuffers(false);

		glPopMatrix();
	}

	void Mesh3D::draw(bool isHighlighted, float x, float y, float z, Anubis::Quaternion rotation, float scale) {
		glPushMatrix();
		glTranslatef(x, y, z);
		const auto matrix = rotation.matrix();
		glMultMatrixf(matrix.data());
		glScalef(scale, scale, scale);
		const float brightness = isHighlighted ? 1.f : 0.75f;
		glColor4f(brightness, brightness, brightness, 1.f);

		drawBuffers(true);

		glPopMatrix();
	}

	std::shared_ptr<Mesh3D> generateCube(float size) {
		std::shared_ptr<Mesh3D> cube = std::make_shared<Mesh3D>();
		// Front face
		cube->_vertices.emplace_back(-size, -size, size, 0.f, 0.f, 1.f, 0.f, 0.f);
		cube->_vertices.emplace_back(size, -size, size, 0.f, 0.f, 1.f, 1.f, 0.f);
		cube->_vertices.emplace_back(size, size, size, 0.f, 0.f, 1.f, 1.f, 1.f);
		cube->_vertices.emplace_back(-size, -size, size, 0.f, 0.f, 1.f, 0.f, 0.f);
		cube->_vertices.emplace_back(size, size, size, 0.f, 0.f, 1.f, 1.f, 1.f);
		cube->_vertices.emplace_back(-size, size, size, 0.f, 0.f, 1.f, 0.f, 1.f);
		// Back face
		cube->_vertices.emplace_back(-size, -size, -size, 0.f, 0.f, -1.f, 1.f, 0.f);
		cube->_vertices.emplace_back(size, -size, -size, 0.f, 0.f, -1.f, 0.f, 0.f);
		cube->_vertices.emplace_back(size, size, -size, 0.f, 0.f, -1.f, 0.f, 1.f);
		cube->_vertices.emplace_back(-size, -size, -size, 0.f, 0.f, -1.f, 1.f, 0.f);
		cube->_vertices.emplace_back(size, size, -size, 0.f, 0.f, -1.f, 0.f, 1.f);
		cube->_vertices.emplace_back(-size, size, -size, 0.f, 0.f, -1.f, 1.f, 1.f);
		// Left face
		cube->_vertices.emplace_back(-size, -size, -size, -1., 0., 0., 0., 0.);
		cube->_vertices.emplace_back(-size, -size, size, -1., 0., 0., 1., 0.);
		cube->_vertices.emplace_back(-size, size, size, -1., 0., 0., 1., 1.);
		cube->_vertices.emplace_back(-size, -size, -size, -1., 0., 0., 0., 0.);
		cube->_vertices.emplace_back(-size, size, size, -1., 0., 0., 1., 1.);
		cube->_vertices.emplace_back(-size, size, -size, -1., 0., 0., 0., 1.);
		// Right face
		cube->_vertices.emplace_back(size, -size, -size, 1., 0., 0., 1., 0.);
		cube->_vertices.emplace_back(size, -size, size, 1., 0., 0., 0., 0.);
		cube->_vertices.emplace_back(size, size, size, 1., 0., 0., 0., 1.);
		cube->_vertices.emplace_back(size, -size, -size, 1., 0., 0., 1., 0.);
		cube->_vertices.emplace_back(size, size, size, 1., 0., 0., 0., 1.);
		cube->_vertices.emplace_back(size, size, -size, 1., 0., 0., 1., 1.);
		// Top face
		cube->_vertices.emplace_back(-size, size, -size, 0., 1., 0., 0., 1.);
		cube->_vertices.emplace_back(-size, size, size, 0., 1., 0., 0., 0.);
		cube->_vertices.emplace_back(size, size, size, 0., 1., 0., 1., 0.);
		cube->_vertices.emplace_back(-size, size, -size, 0., 1., 0., 0., 1.);
		cube->_vertices.emplace_back(size, size, size, 0., 1., 0., 1., 0.);
		cube->_vertices.emplace_back(size, size, -size, 0., 1., 0., 1., 1.);
		// Bottom face
		cube->_vertices.emplace_back(-size, -size, -size, 0., -1., 0., 1., 1.);
		cube->_vertices.emplace_back(-size, -size, size, 0., -1., 0., 1., 0.);
		cube->_vertices.emplace_back(size, -size, size, 0., -1., 0., 0., 0.);
		cube->_vertices.emplace_back(-size, -size, -size, 0., -1., 0., 1., 1.);
		cube->_vertices.emplace_back(size, -size, size, 0., -1., 0., 0., 0.);
		cube->_vertices.emplace_back(size, -size, -size, 0., -1., 0., 0., 1.);

		cube->_indices = {
			0, 1, 2, 3, 4, 5,        // Front
			6, 7, 8, 9, 10, 11,      // Back
			12, 13, 14, 15, 16, 17,  // Left
			18, 19, 20, 21, 22, 23,  // Right
			24, 25, 26, 27, 28, 29,  // Top
			30, 31, 32, 33, 34, 35   // Bottom
		};

		return cube;
	}

	std::shared_ptr<Mesh3D> generateCube(float w, float h, float l) {
		auto cube = std::make_shared<Mesh3D>();

		const float x = w * 0.5f;
		const float y = h * 0.5f;
		const float z = l * 0.5f;

		// Front face (+Z)
		cube->_vertices.emplace_back(-x, -y, z, 0.f, 0.f, 1.f, 0.f, 0.f);
		cube->_vertices.emplace_back(x, -y, z, 0.f, 0.f, 1.f, 1.f, 0.f);
		cube->_vertices.emplace_back(x, y, z, 0.f, 0.f, 1.f, 1.f, 1.f);
		cube->_vertices.emplace_back(-x, -y, z, 0.f, 0.f, 1.f, 0.f, 0.f);
		cube->_vertices.emplace_back(x, y, z, 0.f, 0.f, 1.f, 1.f, 1.f);
		cube->_vertices.emplace_back(-x, y, z, 0.f, 0.f, 1.f, 0.f, 1.f);

		// Back face (-Z)
		cube->_vertices.emplace_back(-x, -y, -z, 0.f, 0.f, -1.f, 1.f, 0.f);
		cube->_vertices.emplace_back(x, y, -z, 0.f, 0.f, -1.f, 0.f, 1.f);
		cube->_vertices.emplace_back(x, -y, -z, 0.f, 0.f, -1.f, 0.f, 0.f);
		cube->_vertices.emplace_back(-x, -y, -z, 0.f, 0.f, -1.f, 1.f, 0.f);
		cube->_vertices.emplace_back(-x, y, -z, 0.f, 0.f, -1.f, 1.f, 1.f);
		cube->_vertices.emplace_back(x, y, -z, 0.f, 0.f, -1.f, 0.f, 1.f);

		// Left face (-X)
		cube->_vertices.emplace_back(-x, -y, -z, -1.f, 0.f, 0.f, 0.f, 0.f);
		cube->_vertices.emplace_back(-x, -y, z, -1.f, 0.f, 0.f, 1.f, 0.f);
		cube->_vertices.emplace_back(-x, y, z, -1.f, 0.f, 0.f, 1.f, 1.f);
		cube->_vertices.emplace_back(-x, -y, -z, -1.f, 0.f, 0.f, 0.f, 0.f);
		cube->_vertices.emplace_back(-x, y, z, -1.f, 0.f, 0.f, 1.f, 1.f);
		cube->_vertices.emplace_back(-x, y, -z, -1.f, 0.f, 0.f, 0.f, 1.f);

		// Right face (+X)
		cube->_vertices.emplace_back(x, -y, -z, 1.f, 0.f, 0.f, 1.f, 0.f);
		cube->_vertices.emplace_back(x, y, z, 1.f, 0.f, 0.f, 0.f, 1.f);
		cube->_vertices.emplace_back(x, -y, z, 1.f, 0.f, 0.f, 0.f, 0.f);
		cube->_vertices.emplace_back(x, -y, -z, 1.f, 0.f, 0.f, 1.f, 0.f);
		cube->_vertices.emplace_back(x, y, -z, 1.f, 0.f, 0.f, 1.f, 1.f);
		cube->_vertices.emplace_back(x, y, z, 1.f, 0.f, 0.f, 0.f, 1.f);

		// Top face (+Y)
		cube->_vertices.emplace_back(-x, y, -z, 0.f, 1.f, 0.f, 0.f, 1.f);
		cube->_vertices.emplace_back(-x, y, z, 0.f, 1.f, 0.f, 0.f, 0.f);
		cube->_vertices.emplace_back(x, y, z, 0.f, 1.f, 0.f, 1.f, 0.f);
		cube->_vertices.emplace_back(-x, y, -z, 0.f, 1.f, 0.f, 0.f, 1.f);
		cube->_vertices.emplace_back(x, y, z, 0.f, 1.f, 0.f, 1.f, 0.f);
		cube->_vertices.emplace_back(x, y, -z, 0.f, 1.f, 0.f, 1.f, 1.f);

		// Bottom face (-Y)
		cube->_vertices.emplace_back(-x, -y, -z, 0.f, -1.f, 0.f, 1.f, 1.f);
		cube->_vertices.emplace_back(x, -y, z, 0.f, -1.f, 0.f, 0.f, 0.f);
		cube->_vertices.emplace_back(-x, -y, z, 0.f, -1.f, 0.f, 1.f, 0.f);
		cube->_vertices.emplace_back(-x, -y, -z, 0.f, -1.f, 0.f, 1.f, 1.f);
		cube->_vertices.emplace_back(x, -y, -z, 0.f, -1.f, 0.f, 0.f, 1.f);
		cube->_vertices.emplace_back(x, -y, z, 0.f, -1.f, 0.f, 0.f, 0.f);

		cube->_indices = {
			0, 1, 2, 3, 4, 5,        // Front
			6, 7, 8, 9, 10, 11,      // Back
			12, 13, 14, 15, 16, 17,  // Left
			18, 19, 20, 21, 22, 23,  // Right
			24, 25, 26, 27, 28, 29,  // Top
			30, 31, 32, 33, 34, 35   // Bottom
		};

		return cube;
	}
}