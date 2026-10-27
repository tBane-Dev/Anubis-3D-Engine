#include "Cursor.hpp"
#include "3D/Scene.hpp"

namespace Anubis {
	Cursor::Cursor() {

		_lastPosition = sf::Mouse::getPosition(*render_window);
		_position = sf::Mouse::getPosition(*render_window);

		_state = CursorState::Idle;
		_rect = sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(0, 0));

		_isDragging = false;
		_isSelecting = false;
	}

	Cursor::~Cursor() {

	}

	void Cursor::update() {
		_lastPosition = _position;
		_position = sf::Mouse::getPosition(*Anubis::render_window);

		if (_isDragging) {

		}
		else if (_isSelecting) {
			scene->_pressed = scene->getModelsInRect(_rect);
		}
	}

	void Cursor::handleEvent(const sf::Event& event) {

		if (const auto* mp = event.getIf<sf::Event::MouseButtonPressed>(); mp && mp->button == sf::Mouse::Button::Left) {

			_position = mp->position;
			_rect.position = _position;
			_rect.size = { 0, 0 };

			int id = scene->getHoveredModelId();

			_isDragging = id >= 0;
			_isSelecting = !_isDragging;

			_state = CursorState::Selecting;

			if (_isSelecting) {
				scene->_pressed.clear();
			}
			else {
				auto model = std::find_if(
					scene->_models.begin(), scene->_models.end(),
					[id](const auto& model) {
						return model->_id == id;
					}
				);

				if (model != scene->_models.end() && std::find(scene->_pressed.begin(), scene->_pressed.end(), *model) == scene->_pressed.end()) {

					scene->_pressed.clear();
					scene->_pressed.push_back(*model);
				}
			}
		}

		if (const auto* mr = event.getIf<sf::Event::MouseButtonReleased>(); mr && mr->button == sf::Mouse::Button::Left) {
			_state = CursorState::Idle;
			_rect.size = sf::Vector2i(0, 0);
			_isDragging = false;
			_isSelecting = false;
		}

		if (const auto* mm = event.getIf<sf::Event::MouseMoved>(); mm && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {

			if (_isDragging) {
			
			}
			else if (_isSelecting) {
				_rect.size = _position - _rect.position;
			}
		}
	}

	void Cursor::draw() {
		if (_rect.size.x != 0 || _rect.size.y != 0) {
			float border = -2.f;
			sf::RectangleShape rect(sf::Vector2f(_rect.size));
			rect.setPosition(sf::Vector2f(_rect.position));
			rect.setFillColor(sf::Color(15.f, 15.f, 127.f, 95.f));
			rect.setOutlineThickness(border);
			rect.setOutlineColor(sf::Color(15.f, 15.f, 127.f, 159.f));
			render_window->draw(rect);
		}
	}

	std::shared_ptr<Cursor> cursor;
}