#pragma once
#include <vector>
#include <SFML/OpenGL.hpp>
#include "3D/Quaternion.hpp"
#include "3D/Vertex3D.hpp"
#include "3D/Skeleton3D.hpp"
#include "TexturesManager.hpp"

namespace Anubis {
	class Mesh3D {
	public:
		std::vector<Vertex3D> _vertices;
		std::vector<std::uint32_t> _indices;
		std::shared_ptr<Texture> _texture;

		GLuint _vertexBuffer = 0;
		GLuint _indexBuffer = 0;
		GLsizei _gpuIndexCount = 0;
		GLuint _vertexArray = 0;

		int _nodeIndex = -1;

		GLuint _jointBuffer = 0;
		GLuint _jointTexture = 0;
		std::size_t _gpuJointCount = 0;

		

		Mesh3D();
		Mesh3D(const Mesh3D&) = delete;
		Mesh3D& operator=(const Mesh3D&) = delete;
		~Mesh3D();

		void setTexture(std::shared_ptr<Texture> texture);
		bool uploadJointMatrices(const std::vector<Matrix4>& matrices);
		void uploadToGPU();

		void drawBuffers(bool textured);
		void drawShape(float x = 0, float y = 0, float z = 0, Anubis::Quaternion rotation = Anubis::Quaternion(1.0f, 0.0f, 0.0f, 0.0f), float scale = 1.0f);
		void draw(bool isHighlighted, float x = 0, float y = 0, float z = 0, Anubis::Quaternion rotation = Anubis::Quaternion(1.0f, 0.0f, 0.0f, 0.0f), float scale = 1.0f);
	};

	std::shared_ptr<Mesh3D> generateCube(float size);
	std::shared_ptr<Mesh3D> generateCube(float w, float h, float l);
}