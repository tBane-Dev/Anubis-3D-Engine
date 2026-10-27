#pragma once
#include "Model3D.hpp"
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics.hpp>

namespace Anubis {

	class Scene {
	public:

		sf::Vector3f _camera;
		float _cameraAngle;
		float _cameraRadius;
		sf::Vector2i _lastCameraMouse;


		std::vector<std::shared_ptr<Model3D>> _models;
		sf::Shader _modelShader;
		
		GLuint _pickingFBO = 0;
		GLuint _pickingColor = 0;
		GLuint _pickingDepth = 0;
		unsigned int _pickingWidth = 0;
		unsigned int _pickingHeight = 0;
		
		std::vector<std::shared_ptr<Model3D>> _hovered;
		std::vector<std::shared_ptr<Model3D>> _pressed;

		Scene();
		~Scene();

		bool initializeModel3DShaders();
		
		void setViewport();
		void setProjection();
		void setCamera();

		
		void addModel(std::shared_ptr<Anubis::Model3D> model);

		void drawShapesOfModels();
		void drawModels();

		bool createPickingBuffer(unsigned int width, unsigned int height);	
		int getHoveredModelId();
		std::vector<std::shared_ptr<Model3D>> getModelsInRect(sf::IntRect rect);

		void cursorHover();
		void handleEvent(const sf::Event& event);
		void update();
		void draw();
	};

	extern std::shared_ptr<Scene> scene;
}