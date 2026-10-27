#include "GUIManager.hpp"
#include "SFML/Graphics.hpp"
#include "RenderWindow.hpp"

namespace Anubis {

	Element::Element() {

	}

	Element::~Element() {

	}

	void Element::cursorHover() {

	}

	void Element::handleEvent(const sf::Event& event) {

	}

	void Element::update() {

	}

	void Element::draw() {

	}

	GUIManager::GUIManager() {
		_view = sf::View(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(render_window->getSize())));

		Element_hovered = nullptr;
		Element_pressed = nullptr;
	}

	GUIManager::~GUIManager() {
	}

	void GUIManager::resize(sf::Vector2i newSize)
	{
		_view = sf::View(sf::FloatRect(sf::Vector2f(0.0f, 0.0f), sf::Vector2f(newSize)));
	}

	void GUIManager::setView() {
		render_window->setView(_view);
	}

	std::shared_ptr<GUIManager> GUI_manager = nullptr;
}
