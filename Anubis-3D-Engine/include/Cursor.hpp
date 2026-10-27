#pragma once
#include "RenderWindow.hpp"

namespace Anubis {

	enum class CursorState { Idle, Selecting };

	class Cursor {
	public:
		sf::Vector2i _lastPosition;
		sf::Vector2i _position;
		CursorState _state;
		sf::IntRect _rect;
		bool _isDragging;
		bool _isSelecting;

		Cursor();
		~Cursor();

		void update();
		void handleEvent(const sf::Event& event);
		void draw();
	};

	extern std::shared_ptr<Cursor> cursor;
}

