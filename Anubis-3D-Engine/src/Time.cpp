#include "Time.hpp"
#include "SFML/Graphics.hpp"

namespace Anubis {
	sf::Clock mainClock;

	sf::Time prevTime;
	sf::Time currentTime;
	sf::Time deltaTime;
}