#include "TexturesManager.hpp"
#include "DebugLog.hpp"

namespace Anubis {

    Texture::Texture(std::wstring path, bool& loadingStatus) {
        _path = path;
        _texture = std::make_shared<sf::Texture>();

        loadingStatus = _texture->loadFromFile(path);
    }

    Texture::Texture(std::wstring path, std::shared_ptr<sf::Image> image) {
        _path = path;

        _texture = std::make_shared<sf::Texture>();
        _texture->loadFromImage(*image);
    }

    Texture::~Texture() {

    }

    sf::Vector2i Texture::getSize() {
        if (_texture == nullptr)
            return sf::Vector2i(0, 0);

        return sf::Vector2i(_texture->getSize());
    }

    TexturesManager::TexturesManager() {
        _textures.clear();

        loadAllTextures();
    }

    TexturesManager::~TexturesManager() {

    }

    std::shared_ptr<Texture> TexturesManager::getTexture(std::wstring path) {
        for (auto& t : _textures) {
            if (t->_path == path)
                return t;
        }

        DebugError(L"texture: " + path + L" no exists");
        return nullptr;
    }

    void TexturesManager::loadTexture(std::wstring path) {

        bool loadingStatus = true;
        std::shared_ptr<Texture> texture = std::make_shared<Texture>(path, loadingStatus);

        if (loadingStatus)
            _textures.push_back(texture);
    }

    void TexturesManager::loadAllTextures() {

        std::vector<std::wstring> texturePaths;

        texturePaths.push_back(L"assets\\tex\\terrain.png");
        texturePaths.push_back(L"assets\\tex\\wall1.png");
        texturePaths.push_back(L"assets\\tex\\wall2.png");
        texturePaths.push_back(L"assets\\tex\\wall3.png");


        // load all textures
        for (auto& path : texturePaths) {
            loadTexture(path);
        }

        // Loaded textures
        
        DebugLog(L"Loading textures:");
        for (auto& path : texturePaths) {
            if (getTexture(path)) {
                DebugStat(path);
            }
        }

        // Failed textures
        bool failed = false;
        for (auto& path : texturePaths) {
            if (!getTexture(path)) {
                if (!failed) {
                    DebugError(L"Failed to load textures:");
                }

                DebugError(path);
                failed = true;
            }
        }

        if (failed) {
            exit(0);
        }

        DebugStat(L"");

        getTexture(L"assets\\tex\\terrain.png")->_texture->setRepeated(true);
    }

    std::shared_ptr<TexturesManager> textures_manager = nullptr;
}