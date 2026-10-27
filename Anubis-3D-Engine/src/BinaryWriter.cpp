#include "BinaryWriter.hpp"
#include "DebugLog.hpp"

BinaryWriter::BinaryWriter(std::ostream& os) : os(os) { }

void BinaryWriter::write_wstring(const std::wstring& str) {
    uint16_t len = static_cast<uint16_t>(str.size());
    os.write(reinterpret_cast<const char*>(&len), sizeof(len));
    os.write(reinterpret_cast<const char*>(str.data()), len * sizeof(wchar_t));
}

void BinaryWriter::write_int32(int32_t val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(int32_t));
}

void BinaryWriter::write_int16(int16_t val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(int16_t));
}

void BinaryWriter::write_int8(int8_t val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(int8_t));
}

void BinaryWriter::write_float(float val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(float));
}

void BinaryWriter::write_Vector2i(sf::Vector2i val) {
    write_int32(val.x);
    write_int32(val.y);
}

void BinaryWriter::write_Image(sf::Image image) {
    auto pngData = image.saveToMemory("png");
    
    if (!pngData) {
        DebugLog(L"Cannot compress image to PNG");
        write_int32(0);
        return;
    }

    write_int32(pngData->size());
    os.write(reinterpret_cast<const char*>(pngData->data()), pngData->size());

}
