#include <io.h>       // _setmode, _fileno
#include <fcntl.h>   // _O_U16TEXT
#include <stdio.h>   // stdout

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include "gl/glu.h"

#include "Intro.hpp"

#include "3D/OpenGLBuffers.hpp"
#include "3D/Vertex3D.hpp"
#include "3D/Mesh3D.hpp" 
#include "3D/Model3D.hpp"
#include "3D/Scene.hpp"

#include "RenderWindow.hpp"
#include "DebugLog.hpp"
#include "Time.hpp"
#include "Cursor.hpp"
#include "TexturesManager.hpp"
#include "GUIManager.hpp"

sf::Font font;

int main()
{
    (void)_setmode(_fileno(stdout), _O_U16TEXT); // wide char UTF-16 output

    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    
    Anubis::render_window = std::make_unique<sf::RenderWindow>(sf::VideoMode(sf::Vector2u(1920, 1080), 32u), "Anubis-3D-Engine", sf::State::Windowed, settings);
    Anubis::render_window->setVerticalSyncEnabled(false);
    Anubis::render_window->setFramerateLimit(0);

    auto* version = glGetString(GL_VERSION);
    auto* renderer = glGetString(GL_RENDERER);

    if (version) {
        std::string text(reinterpret_cast<const char*>(version));
        DebugStat(L"OpenGL: " + std::wstring(text.begin(), text.end()));
    }

    if (renderer) {
        std::string text(reinterpret_cast<const char*>(renderer));
        DebugStat(L"Renderer: " + std::wstring(text.begin(), text.end()));
    }

    DebugStat(L"");

    Anubis::render_window->clear(sf::Color::Black);
    Anubis::render_window->display();

    while (const std::optional event = Anubis::render_window->waitEvent()) {

        if (event->is<sf::Event::Closed>()) {
            Anubis::render_window->close();
            exit(0);
        }

        if (const auto* kp = event->getIf<sf::Event::KeyPressed>(); kp && kp->code == sf::Keyboard::Key::Space) {
            break;
        }
    }

    Anubis::renderIntro();

    Anubis::render_window->resetGLStates();

    // OpenGL Buffers initialize :-)
    if (!Anubis::GL::initialize())
        return EXIT_FAILURE;

    if (!font.openFromFile("C:/Windows/Fonts/consola.ttf")) {
        DebugError(L"Failed to load font");
        exit(0);
    }

    Anubis::textures_manager = std::make_shared<Anubis::TexturesManager>();
    Anubis::cursor = std::make_shared<Anubis::Cursor>();
    
    // create scene
    Anubis::scene = std::make_shared<Anubis::Scene>();
    
    auto size = Anubis::render_window->getSize();
    if (!Anubis::scene->createPickingBuffer(size.x, size.y)) {
        DebugError(L"Failed to create Pisking Buffer");
        exit(0);
    }
        

    if (!Anubis::scene->initializeModel3DShaders()) {
        DebugError(L"Failed to Initialize Model3D Shaders");
        exit(0);
    }
        

    // create models and add them to scene

    {
        // add terrain
        std::shared_ptr<Anubis::Model3D> terrain = std::make_shared<Anubis::Model3D>();
        std::shared_ptr<Anubis::Mesh3D> terrain_mesh = Anubis::generateCube(5.0f, 0.25f, 5.0f);
        terrain_mesh->setTexture(Anubis::textures_manager->getTexture(L"assets\\tex\\terrain.png"));
        terrain_mesh->uploadToGPU();
        terrain->addMesh(terrain_mesh);
        terrain->_y = -1.f;
        Anubis::scene->addModel(terrain);
    }

    //{
    //    // add left cube
    //    std::shared_ptr<Anubis::Model3D> cube = std::make_shared<Anubis::Model3D>();
    //    std::shared_ptr<Anubis::Mesh3D> cube_mesh = Anubis::generateCube(0.5f);
    //    cube_mesh->setTexture(Anubis::textures_manager->getTexture(L"assets\\tex\\wall2.png"));
    //    cube_mesh->uploadToGPU();
    //    cube->addMesh(cube_mesh);
    //    cube->_x = -2.0f;
    //    cube->_isSelectable = true;
    //    float angle = 30.f * 3.14159265f / 180.f;
    //    cube->_rotationDelta = Anubis::Quaternion{
    //        std::cos(angle * 0.5f),
    //        0.f,
    //        std::sin(angle * 0.5f),
    //        0.f
    //    };
    //    cube->_rotationSpeed = 1.f;
    //    Anubis::scene->addModel(cube);
    //}
    
    {
        std::shared_ptr<Anubis::Model3D> troll = Anubis::openModel3DFromFile(L"assets\\mdl\\troll.glb");
        troll->_scale = 1.0f;
        troll->_x = -2.0f;
        troll->_y = -1.0f + 0.125f;
        troll->_isSelectable = true;
        Anubis::scene->addModel(troll);
    }

    {
        std::shared_ptr<Anubis::Model3D> golem = Anubis::openModel3DFromFile(L"assets\\mdl\\golem.glb");
        golem->_scale = 1.0f;
        golem->_y = -1.0f + 0.125f;
        golem->_isSelectable = true;
        Anubis::scene->addModel(golem);
    }

    {
        std::shared_ptr<Anubis::Model3D> beaked = Anubis::openModel3DFromFile(L"assets\\mdl\\beaked.glb");
        beaked->_scale = 1.0f;
        beaked->_x = 2.0f;
        beaked->_y = -1.0f + 0.125f;
        beaked->_isSelectable = true;
        Anubis::scene->addModel(beaked);
    }


    // Exclude loading time from the first frame.
    Anubis::mainClock.restart();
    Anubis::deltaTime = sf::Time::Zero;
    
    // OpenGL init
    glClearDepth(1.f);
    glClearColor(0.25f, 0.25f, 0.25f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // init FPS clock
    sf::Clock FPSClock;
    int frames = 0;
    float elapsed = 0.0f;
    float FPS = 0;
    float ms = 0;

    while (Anubis::render_window->isOpen())
    {
        Anubis::prevTime = Anubis::currentTime;
        Anubis::currentTime = Anubis::mainClock.getElapsedTime();
        Anubis::deltaTime = Anubis::currentTime - Anubis::prevTime;
    
        // FPS TIMER
        float dtFPS = FPSClock.restart().asSeconds();
        frames++;
        elapsed += dtFPS;
    
        if (elapsed >= 0.5f) {
            FPS = frames / elapsed;
            ms = 1000.0f / FPS;
    
            std::ostringstream ss;
            ss << "Anubis-3D-Engine - " << std::fixed << std::setprecision(2) << FPS << " FPS / " << std::setprecision(3) << ms << " ms";
            Anubis::render_window->setTitle(ss.str());
            frames = 0;
            elapsed = 0.0f;
        }
    
    
    
        // Cursor Hover
        Anubis::cursor->update();
        Anubis::scene->cursorHover();
    
        // Handle Events
        while (const std::optional event = Anubis::render_window->pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                Anubis::render_window->close();
                exit(0);
            }
    
            // TO-DO - to delete
            if (const auto* pressed = event->getIf<sf::Event::KeyPressed>(); pressed && pressed->code == sf::Keyboard::Key::Space) {
                Anubis::scene->_models[2]->_rotation = Anubis::Quaternion(1.0f, 0.f, 0.f, 0.f);
            }
            //
            
            Anubis::cursor->handleEvent(*event);
            Anubis::scene->handleEvent(*event);
    
        }
    
        // Update
        Anubis::scene->update();
    
        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
        Anubis::scene->draw();
    
        
	    // draw SFML 3.1
        Anubis::render_window->pushGLStates();
    
        Anubis::cursor->draw();

        float border = 4.0f;
        sf::RectangleShape rect(sf::Vector2f(448, 64));
        rect.setFillColor(sf::Color(47, 47, 47));
        rect.setOutlineColor(sf::Color::Black);
        rect.setOutlineThickness(border);
        rect.setPosition(sf::Vector2f(border, border));
        Anubis::render_window->draw(rect);
    
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << FPS << " FPS / " << std::setprecision(3) << ms << " ms";
        sf::Text fps(font, ss.str(), 32);
        fps.setPosition(sf::Vector2f(16.0f, 16.0f));
        fps.setFillColor(sf::Color::White);
        Anubis::render_window->draw(fps);
     
        Anubis::render_window->popGLStates();
    
        // render submit
        Anubis::render_window->display();
    }

    return 0;
}
