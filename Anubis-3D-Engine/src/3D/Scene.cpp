#include "3D/Scene.hpp"
#include <SFML/OpenGL.hpp>
#include <GL/glu.h>
#include "DebugLog.hpp"
#include "RenderWindow.hpp"
#include "Cursor.hpp"
#include "3D/OpenGLBuffers.hpp"

namespace Anubis {

    Scene::Scene() {

        _cameraAngle = 0.f; // radians
        _cameraRadius = 5.f;
        _lastCameraMouse = -sf::Vector2i(0, 0);

        _camera.x = std::sin(_cameraAngle) * _cameraRadius;
        _camera.y = 0.f;
        _camera.z = std::cos(_cameraAngle) * _cameraRadius;

    }

    Scene::~Scene() {

        if (_pickingFBO != 0)
            GL::deleteFramebuffers(1, &_pickingFBO);

        if (_pickingColor != 0)
            glDeleteTextures(1, &_pickingColor);

        if (_pickingDepth != 0)
            glDeleteTextures(1, &_pickingDepth);
    }

    bool Scene::initializeModel3DShaders() {

        if (!sf::Shader::isAvailable()) {
            DebugError(L"Shaders are not supported");
            return false;
        }

        if (!_modelShader.loadFromFile("assets\\shr\\model3d.vert", "assets\\shr\\model3d.frag")) {
            DebugError(L"Failed to load model3D shaders");
            return false;
        }

        // Use separate texture units for different sampler types.
        _modelShader.setUniform("uTexture", 0);
        _modelShader.setUniform("uJointMatrices", 1);

        // Start with static mesh rendering.
        _modelShader.setUniform("uSkinned", false);
        _modelShader.setUniform("uPicking", false);
        _modelShader.setUniform("uTextured", false);
        _modelShader.setUniform("uTint", sf::Glsl::Vec4(1.f, 1.f, 1.f, 1.f));
        _modelShader.setUniform("uPickColor", sf::Glsl::Vec3(0.f, 0.f, 0.f));

        DebugLog(L"Model3D shaders loaded");
        return true;
    }

    void Scene::setViewport()
    {
        glViewport(0, 0, Anubis::render_window->getSize().x, Anubis::render_window->getSize().y);
    }

    void Scene::setProjection()
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        gluPerspective(
            60.0,
            double(Anubis::render_window->getSize().x) / double(Anubis::render_window->getSize().y),
            0.1,
            100.0
        );
    }

    void Scene::setCamera()
    {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float toDegrees = 180.f / 3.14159265f;

        glRotatef(-_cameraAngle * toDegrees, 0.f, 1.f, 0.f);
        glTranslatef(-_camera.x, -_camera.y, -_camera.z);
    }

    void Scene::addModel(std::shared_ptr<Model3D> model) {
        _models.push_back(model);
    }

    void Scene::drawShapesOfModels() {
        for (auto& model : _models) {
            model->drawShape();
        }
    }

    void Scene::drawModels() {
        for (auto& model : _models) {
            model->draw();
        }
    }

    bool Scene::createPickingBuffer(unsigned int width, unsigned int height) {
        if (width == 0 || height == 0)
            return false;

        if (_pickingFBO != 0 && _pickingWidth == width && _pickingHeight == height)
            return true;

        if (_pickingFBO == 0)
            GL::genFramebuffers(1, &_pickingFBO);

        if (_pickingColor == 0)
            glGenTextures(1, &_pickingColor);

        if (_pickingDepth == 0)
            glGenTextures(1, &_pickingDepth);

        glBindTexture(GL_TEXTURE_2D, _pickingColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8,
            width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr
        );

        constexpr GLenum depthComponent24 = 0x81A6;

        glBindTexture(GL_TEXTURE_2D, _pickingDepth);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexImage2D(
            GL_TEXTURE_2D, 0, depthComponent24,
            width, height, 0,
            GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr
        );

        GL::bindFramebuffer(GL::Framebuffer, _pickingFBO);

        GL::framebufferTexture2D(
            GL::Framebuffer, GL::ColorAttachment0,
            GL_TEXTURE_2D, _pickingColor, 0
        );

        GL::framebufferTexture2D(
            GL::Framebuffer, GL::DepthAttachment,
            GL_TEXTURE_2D, _pickingDepth, 0
        );

        glDrawBuffer(GL::ColorAttachment0);
        glReadBuffer(GL::ColorAttachment0);

        const bool complete = GL::checkFramebufferStatus(GL::Framebuffer) == GL::FramebufferComplete;

        GL::bindFramebuffer(GL::Framebuffer, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (!complete) {
            DebugError(L"Picking framebuffer is incomplete");
            _pickingWidth = 0;
            _pickingHeight = 0;
            return false;
        }

        _pickingWidth = width;
        _pickingHeight = height;

        DebugStat(L"Picking framebuffer created");
        return true;
    }


    int Scene::getHoveredModelId() {
        const auto size = Anubis::render_window->getSize();
        const auto mouse = Anubis::cursor->_position;

        if (size.x == 0 || size.y == 0)
            return -1;

        if (mouse.x < 0 || mouse.y < 0 ||
            mouse.x >= static_cast<int>(size.x) ||
            mouse.y >= static_cast<int>(size.y))
            return -1;

        if (!createPickingBuffer(size.x, size.y))
            return -1;

        const int pixelX = mouse.x;
        const int pixelY = static_cast<int>(size.y) - mouse.y - 1;

        // Preserve OpenGL state.
        GLint previousMatrixMode = 0;
        glGetIntegerv(GL_MATRIX_MODE, &previousMatrixMode);

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        GL::bindFramebuffer(GL::Framebuffer, _pickingFBO);

        glDrawBuffer(GL::ColorAttachment0);
        glReadBuffer(GL::ColorAttachment0);

        setViewport();
        setProjection();
        setCamera();

        // Preserve exact ID colors.
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDisable(GL_DITHER);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_FOG);
        glDisable(GL_COLOR_LOGIC_OP);

        constexpr GLenum framebufferSRGB = 0x8DB9;
        constexpr GLenum multisample = 0x809D;

        glDisable(framebufferSRGB);
        glDisable(multisample);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Only the pixel under the cursor needs a result.
        glEnable(GL_SCISSOR_TEST);
        glScissor(pixelX, pixelY, 1, 1);

        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (const auto& model : _models) {
            if (model->_id < 0 || model->_id > 0xFFFFFE || !model->_isSelectable)
                continue;

            const GLuint encodedId = static_cast<GLuint>(model->_id) + 1u;

            glColor3ub(
                static_cast<GLubyte>(encodedId & 0xFF),
                static_cast<GLubyte>((encodedId >> 8) & 0xFF),
                static_cast<GLubyte>((encodedId >> 16) & 0xFF)
            );

            model->drawShape();
        }

        GLubyte pixel[4] = {};

        constexpr GLenum pixelPackBuffer = 0x88EB;
        GL::bindBuffer(pixelPackBuffer, 0);

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

        glReadPixels(
            pixelX, pixelY,
            1, 1,
            GL_RGBA, GL_UNSIGNED_BYTE,
            pixel
        );

        GL::bindFramebuffer(GL::Framebuffer, 0);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glMatrixMode(previousMatrixMode);

        glPopClientAttrib();
        glPopAttrib();

        const GLuint encodedId =
            static_cast<GLuint>(pixel[0]) |
            (static_cast<GLuint>(pixel[1]) << 8) |
            (static_cast<GLuint>(pixel[2]) << 16);

        return encodedId == 0
            ? -1
            : static_cast<int>(encodedId - 1u);
    }

    std::vector<std::shared_ptr<Model3D>> Scene::getModelsInRect(sf::IntRect rect) {

        std::vector<std::shared_ptr<Model3D>> selected;

        const auto size = Anubis::render_window->getSize();

        if (size.x == 0 || size.y == 0)
            return selected;

        const int width = static_cast<int>(size.x);
        const int height = static_cast<int>(size.y);

        sf::Vector2i start = rect.position;
        sf::Vector2i end = rect.position + rect.size;

        const int left = std::clamp(std::min(start.x, end.x), 0, width);
        const int right = std::clamp(std::max(start.x, end.x), 0, width);
        const int top = std::clamp(std::min(start.y, end.y), 0, height);
        const int bottom = std::clamp(std::max(start.y, end.y), 0, height);

        if (left == right || top == bottom)
            return selected;

        if (!createPickingBuffer(size.x, size.y))
            return selected;

        // Keep one query for each model.
        std::vector<GLuint> queries(_models.size(), 0);
        selected.reserve(_models.size());

        GLint previousMatrixMode = 0;
        glGetIntegerv(GL_MATRIX_MODE, &previousMatrixMode);

        glPushAttrib(GL_ALL_ATTRIB_BITS);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        GL::bindFramebuffer(GL::Framebuffer, _pickingFBO);

        setViewport();
        setProjection();
        setCamera();

        glEnable(GL_SCISSOR_TEST);
        glScissor(left, height - bottom, right - left, bottom - top);

        // Include models hidden behind other models.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_POLYGON_STIPPLE);

        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Only count samples; do not modify the framebuffer.
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);

        // Submit every query before reading any results.
        for (std::size_t i = 0; i < _models.size(); ++i) {
            if (!_models[i])
                continue;

            GL::genQueries(1, &queries[i]);

            GL::beginQuery(GL::AnySamplesPassed, queries[i]);
            _models[i]->drawShape();
            GL::endQuery(GL::AnySamplesPassed);
        }

        // Restore rendering state.
        GL::bindFramebuffer(GL::Framebuffer, 0);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glMatrixMode(previousMatrixMode);
        glPopAttrib();

        // Read results. This can wait for the GPU.
        for (std::size_t i = 0; i < _models.size(); ++i) {
            if (queries[i] == 0)
                continue;

            GLuint passed = GL_FALSE;

            GL::getQueryObjectuiv(
                queries[i],
                GL::QueryResult,
                &passed
            );

            if (passed != GL_FALSE && _models[i]->_isSelectable)
                selected.push_back(_models[i]);

            GL::deleteQueries(1, &queries[i]);
        }

        return selected;
    }

    void Scene::cursorHover() {

        _hovered.clear();

        if (cursor->_rect.size.x == 0 && cursor->_rect.size.y == 0) {
            int id = getHoveredModelId();

            if (id >= 0) {
                auto it = std::find_if(_models.begin(), _models.end(),
                    [id](const auto& model) {
                        return model->_id == id;
                    });

                if (it != _models.end())
                    _hovered.push_back(*it);
            }
        }
        else {

            _hovered = scene->getModelsInRect(cursor->_rect);
        }

        
    }

    void Scene::handleEvent(const sf::Event& event) {

        if (const auto* scroll = event.getIf<sf::Event::MouseWheelScrolled>(); scroll) {
            _cameraRadius -= scroll->delta * 0.5f;

            _camera.x = std::sin(_cameraAngle) * _cameraRadius;
            _camera.y = 0.f;
            _camera.z = std::cos(_cameraAngle) * _cameraRadius;
            return;
        }

        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>(); pressed && pressed->button == sf::Mouse::Button::Middle)
        {
            _lastCameraMouse = pressed->position;
            return;
        }

        if (const auto* moved = event.getIf<sf::Event::MouseMoved>(); moved && sf::Mouse::isButtonPressed(sf::Mouse::Button::Middle))
        {
            sf::Vector2i delta = moved->position - _lastCameraMouse;
            _lastCameraMouse = moved->position;

            float sensitivity = 0.005f; // Radiany na piksel.
            _cameraAngle -= float(delta.x) * sensitivity;

            _camera.x = std::sin(_cameraAngle) * _cameraRadius;
            _camera.y = 0.f;
            _camera.z = std::cos(_cameraAngle) * _cameraRadius;
            return;
        }
        for (auto& model : _models) {
            model->handleEvent(event);
        }

    }

    void Scene::update() {
        for (auto& model : _models) {
            model->update();
        }
    }

    void Scene::draw() {

        setViewport();
        setProjection();
        setCamera();

        drawModels();
    }

    std::shared_ptr<Scene> scene;
}

