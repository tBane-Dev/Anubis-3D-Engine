#pragma once
#include <string>

class JsonReader {
public:

	std::string _text;
	int _position = 0;

	JsonReader(std::string text);
	~JsonReader();

	void skipWhitespace();
	bool expect(char expected);
	char peek();
	bool skipValue();

	bool findField(const std::string& name, bool required = true);
	bool findRootField(const std::string& name);
	bool enterArrayElement(int index);

	bool readInt(int& value);
	bool readFloat(float& value);
	std::string readString();
	std::wstring readWstring();
};