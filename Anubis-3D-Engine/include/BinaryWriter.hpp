#pragma once
#include <iostream>
#include <fstream>
#include <SFML/Graphics.hpp>

class BinaryWriter {
    std::ostream& os;
public:
    BinaryWriter(std::ostream& os);

    void write_wstring(const std::wstring& str);
    void write_int32(int32_t val);
    void write_int16(int16_t val);
    void write_int8(int8_t val);
	void write_float(float val);
    void write_Vector2i(sf::Vector2i val);
    void write_Image(sf::Image image);
};
