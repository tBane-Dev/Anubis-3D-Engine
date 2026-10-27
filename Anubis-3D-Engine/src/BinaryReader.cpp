#include "BinaryReader.hpp"
#include "DebugLog.hpp"

BinaryReader::BinaryReader(std::ifstream& is) : is(is) { }

std::wstring BinaryReader::read_wstring() {
    uint16_t len = 0;
    is.read(reinterpret_cast<char*>(&len), sizeof(len));

    std::wstring str(len, L'\0');
    is.read(reinterpret_cast<char*>(str.data()), len * sizeof(wchar_t));

    return str;
}

uint32_t BinaryReader::read_uint32() {
    uint32_t value;
    is.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
    return value;
}

int32_t BinaryReader::read_int32() {
    int32_t value;
    is.read(reinterpret_cast<char*>(&value), sizeof(int32_t));
    return value;
}

int16_t BinaryReader::read_int16() {
    int16_t value;
    is.read(reinterpret_cast<char*>(&value), sizeof(int16_t));
    return value;
}

int8_t BinaryReader::read_int8() {
    int8_t value;
    is.read(reinterpret_cast<char*>(&value), sizeof(int8_t));
    return value;
}

float BinaryReader::read_float() {
    float value;
    is.read(reinterpret_cast<char*>(&value), sizeof(float));
    return value;
}

sf::Vector2i BinaryReader::read_Vector2i() {
    sf::Vector2i value;
	value.x = read_int32();
	value.y = read_int32();
    return value;
}

sf::Image BinaryReader::read_Image() {

    int imageDataSize = read_int32();

    std::vector<uint8_t> imageData(imageDataSize);
    is.read(reinterpret_cast<char*>(imageData.data()), imageDataSize);
    
    sf::Image image;
    if (!image.loadFromMemory(imageData.data(), imageDataSize)) {
        DebugLog(L"Failed to load image from memory");
    }

    return image;
}