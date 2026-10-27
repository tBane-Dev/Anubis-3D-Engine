#pragma once
#include <SFML/Graphics.hpp>
#include "RenderWindow.hpp"
#include <iostream>

namespace Anubis {

	enum class intro_states { start, logo_entry_anim, logo_hidding, end };

	class Intro {

	public:
		intro_states state;

		sf::Font font;

		float logo_scale;
		sf::Texture logo_texture;
		std::unique_ptr<sf::Text> logo_text;

		sf::Time start_time;

		Intro(std::wstring logo_path, float scale) {
			logo_texture = sf::Texture(logo_path);

			sf::Vector2f screen_size = sf::Vector2f(render_window->getSize().x, render_window->getSize().y);

			if (render_window->getSize().x < render_window->getSize().y) {
				logo_scale = ((float)render_window->getSize().x * 0.4f) / float(logo_texture.getSize().x) * scale;
			}
			else {
				logo_scale = ((float)render_window->getSize().y * 0.30f) / float(logo_texture.getSize().y) * scale;
			}
		}

		Intro(std::wstring logo_path, std::wstring font_path, std::wstring text) : Intro(logo_path, 1.f) {

			if (!font.openFromFile(font_path))
				std::wcout << L"Failed to load font from " << font_path << "\n";

			logo_text = std::make_unique<sf::Text>(font, text, 260.f * logo_scale);
			logo_text->setOrigin(sf::Vector2f(logo_text->getGlobalBounds().size.x / 2, logo_text->getGlobalBounds().size.y / 2));
			logo_text->setFillColor(sf::Color::White);

		}

		~Intro() {}

		void play() {
			state = intro_states::start;
			sf::Clock clock;
			start_time = clock.getElapsedTime();

			// ustaw pozycje
			float center_x = render_window->getSize().x / 2.0f;
			float center_y = render_window->getSize().y / 2.0f;

			sf::Sprite logo_sprite(logo_texture);
			logo_sprite.setScale(sf::Vector2f(logo_scale, logo_scale));
			logo_sprite.setOrigin(sf::Vector2f(logo_texture.getSize().x / 2, logo_texture.getSize().y / 2));

			float logo_h = logo_sprite.getGlobalBounds().size.y;
			float text_h = logo_text ? logo_text->getGlobalBounds().size.y : 0.0f;
			float total_h = logo_h + (logo_text ? text_h : 0.0f);
			float offset = 24.0f * logo_scale;

			sf::Vector2f sprtitePosition = sf::Vector2f(center_x, center_y - total_h / 2 + logo_h / 2);
			if (logo_text) sprtitePosition.y -= offset / 2;

			logo_sprite.setPosition(sprtitePosition);
			if (logo_text) {
				logo_text->setPosition(sf::Vector2f(center_x, center_y - total_h / 2 + logo_h + offset));
			}

			render_window->clear(sf::Color::Black);
			render_window->display();


			state = intro_states::logo_entry_anim;

			while (state != intro_states::end) {

				while (const std::optional event = render_window->pollEvent()) {

					if (event->is<sf::Event::Closed>()) {
						render_window->close();
						exit(0);
					}
				}

				if (state == intro_states::logo_entry_anim) {
					float col_alpha = (clock.getElapsedTime() - start_time).asSeconds() * 50000.0f / 255.0f;

					logo_sprite.setColor(sf::Color(255.0f, 255.0f, 255.0f, col_alpha));

					if (logo_text) {
						logo_text->setFillColor(sf::Color(255.0f, 255.0f, 255.0f, col_alpha));
					}

					render_window->clear(sf::Color::Black);
					render_window->draw(logo_sprite);
					if (logo_text) render_window->draw(*logo_text);
					render_window->display();

					if (col_alpha > 255) {
						state = intro_states::logo_hidding;
						start_time = clock.getElapsedTime();
					}
				}
				else if (state == intro_states::logo_hidding) {
					float col_alpha = 255 - (clock.getElapsedTime() - start_time).asSeconds() * 100000.0f / 255.0f;
					logo_sprite.setColor(sf::Color(255.0f, 255.0f, 255.0f, col_alpha));

					if (logo_text) {
						logo_text->setFillColor(sf::Color(255.0f, 255.0f, 255.0f, col_alpha));
					}

					render_window->clear(sf::Color::Black);
					render_window->draw(logo_sprite);
					if (logo_text) render_window->draw(*logo_text);
					render_window->display();

					if (col_alpha <= 0) {
						state = intro_states::end;
					}
				}
			}
		}

	};

	void renderIntro() {
		std::shared_ptr<Intro> intro_sfml = std::make_shared<Intro>(L"assets\\tex\\intro\\sfml-icon-big.png", L"C:\\Windows\\Fonts\\arial.ttf", L"SFML");
		intro_sfml->play();

		std::shared_ptr<Intro> intro_AnubisEngine = std::make_shared<Intro>(L"assets\\tex\\intro\\logo-AnubisEngine.png", 2.0f);
		intro_AnubisEngine->play();

		std::shared_ptr<Intro> intro_tBane = std::make_shared<Intro>(L"assets\\tex\\intro\\logo-tBane.png", 1.0f);
		intro_tBane->play();

		
	}
}