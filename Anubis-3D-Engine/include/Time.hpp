#pragma once
#include "SFML/Graphics.hpp"

namespace Anubis {
	extern sf::Clock mainClock;	// main clock

	extern sf::Time prevTime;
	extern sf::Time currentTime;
	extern sf::Time deltaTime;
}