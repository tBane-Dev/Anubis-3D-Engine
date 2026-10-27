#pragma once
#include <SFML/Graphics.hpp>

namespace Anubis {
    class Texture {
    public:
        std::wstring _path;
        std::shared_ptr<sf::Texture> _texture;

        Texture(std::wstring path, bool& loadingStatus);
        Texture(std::wstring path, std::shared_ptr<sf::Image> image);
        ~Texture();
        sf::Vector2i getSize();
    };

    class TexturesManager {
    public:
        std::vector<std::shared_ptr<Texture>> _textures;

        TexturesManager();
        ~TexturesManager();

        std::shared_ptr<Texture> getTexture(std::wstring path);
        void loadTexture(std::wstring path);
        void loadAllTextures();
    };

    extern std::shared_ptr<TexturesManager> textures_manager;
}