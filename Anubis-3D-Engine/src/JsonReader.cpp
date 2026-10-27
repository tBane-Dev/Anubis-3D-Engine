#include "JsonReader.hpp"
#include "DebugLog.hpp"

JsonReader::JsonReader(std::string text) {
	_text = text;
}

JsonReader::~JsonReader() {

}

void JsonReader::skipWhitespace() {
	while (_position < _text.size()) {
		char c = _text[_position];

		if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
			break;

		++_position;
	}
}

bool JsonReader::expect(char expected) {
	skipWhitespace();

	if (_position >= _text.size() || _text[_position] != expected) {
		DebugError(L"Nieoczekiwany znak JSON");
		return false;
	}

	++_position;
	return true;
}

char JsonReader::peek() {
	skipWhitespace();

	if (_position >= _text.size())
		return '\0';

	return _text[_position];
}

bool JsonReader::skipValue() {
	char c = peek();

	// Napis
	if (c == '"') {
		readString();
		return true;
	}

	// Obiekt
	if (c == '{') {
		++_position;

		if (peek() == '}') {
			++_position;
			return true;
		}

		while (true) {
			if (peek() != '"') {
				DebugError(L"Oczekiwano nazwy pola JSON");
				return false;
			}

			readString();

			if (!expect(':') || !skipValue())
				return false;

			if (peek() == '}') {
				++_position;
				return true;
			}

			if (!expect(','))
				return false;
		}
	}

	// Tablica
	if (c == '[') {
		++_position;

		if (peek() == ']') {
			++_position;
			return true;
		}

		while (true) {
			if (!skipValue())
				return false;

			if (peek() == ']') {
				++_position;
				return true;
			}

			if (!expect(','))
				return false;
		}
	}

	// Liczba, true, false lub null:
	// na tym etapie pomijamy token bez sprawdzania jego składni.
	int start = _position;

	while (_position < _text.size()) {
		c = _text[_position];

		if (c == ',' || c == '}' || c == ']' ||
			c == ' ' || c == '\t' || c == '\r' || c == '\n')
			break;

		++_position;
	}

	if (_position == start) {
		DebugError(L"Oczekiwano wartości JSON");
		return false;
	}

	return true;
}


bool JsonReader::findField(const std::string& name, bool required) {
	while (peek() != '}') {
		if (peek() != '"') {
			DebugError(L"Oczekiwano nazwy pola JSON");
			return false;
		}

		std::string fieldName = readString();

		if (!expect(':'))
			return false;

		if (fieldName == name)
			return true;

		if (!skipValue())
			return false;

		if (peek() == '}')
			break;

		if (!expect(','))
			return false;
	}

	if (required)
		DebugError(L"JSON field not found");

	return false;
}

bool JsonReader::findRootField(const std::string& name) {
	_position = 0;

	if (!expect('{'))
		return false;

	return findField(name);
}

bool JsonReader::enterArrayElement(int index) {
	if (index < 0)
		return false;

	if (!expect('['))
		return false;

	for (int i = 0; i < index; ++i) {
		if (peek() == ']' || peek() == '\0') {
			DebugError(L"Indeks poza tablicą JSON");
			return false;
		}

		if (!skipValue() || !expect(','))
			return false;
	}

	if (peek() == ']' || peek() == '\0') {
		DebugError(L"Indeks poza tablicą JSON");
		return false;
	}

	return true;
}

bool JsonReader::readInt(int& value) {
	skipWhitespace();

	const char* begin = _text.data() + _position;
	const char* end = _text.data() + _text.size();

	int result = 0;
	auto [next, error] = std::from_chars(begin, end, result);

	if (error != std::errc{} || result < 0) {
		DebugError(L"Oczekiwano nieujemnej liczby całkowitej");
		return false;
	}

	// Nie akceptujemy np. 12.5 lub 12abc.
	if (next != end) {
		char c = *next;

		if (c != ',' && c != '}' && c != ']' &&
			c != ' ' && c != '\t' && c != '\r' && c != '\n') {
			DebugError(L"Nieprawidłowy koniec liczby");
			return false;
		}
	}

	if (*begin == '0' && next - begin > 1) {
		DebugError(L"Niedozwolone zero na początku liczby");
		return false;
	}

	_position += static_cast<int>(next - begin);
	value = result;
	return true;
}

bool JsonReader::readFloat(float& value) {
	skipWhitespace();

	const auto start = static_cast<std::size_t>(_position);
	auto position = start;

	const auto isDigit = [](char c) {
		return c >= '0' && c <= '9';
		};

	// Read the optional minus sign.
	if (position < _text.size() && _text[position] == '-')
		++position;

	if (position >= _text.size()) {
		DebugError(L"Expected a JSON number");
		return false;
	}

	// Read the integer part and reject leading zeros.
	if (_text[position] == '0') {
		++position;

		if (position < _text.size() && isDigit(_text[position])) {
			DebugError(L"Leading zeros are not allowed");
			return false;
		}
	}
	else if (_text[position] >= '1' && _text[position] <= '9') {
		while (position < _text.size() && isDigit(_text[position]))
			++position;
	}
	else {
		DebugError(L"Invalid JSON number");
		return false;
	}

	// Read the optional fractional part.
	if (position < _text.size() && _text[position] == '.') {
		++position;

		const auto digitsStart = position;

		while (position < _text.size() && isDigit(_text[position]))
			++position;

		if (position == digitsStart) {
			DebugError(L"Expected digits after the decimal point");
			return false;
		}
	}

	// Read the optional exponent.
	if (position < _text.size() &&
		(_text[position] == 'e' || _text[position] == 'E')) {
		++position;

		if (position < _text.size() &&
			(_text[position] == '+' || _text[position] == '-')) {
			++position;
		}

		const auto digitsStart = position;

		while (position < _text.size() && isDigit(_text[position]))
			++position;

		if (position == digitsStart) {
			DebugError(L"Expected exponent digits");
			return false;
		}
	}

	// Reject unexpected characters after the number.
	if (position < _text.size()) {
		const char c = _text[position];

		if (c != ',' && c != ']' && c != '}' &&
			c != ' ' && c != '\t' && c != '\r' && c != '\n') {
			DebugError(L"Invalid character after JSON number");
			return false;
		}
	}

	const char* begin = _text.data() + start;
	const char* end = _text.data() + position;

	float result = 0.f;

	const auto [next, error] = std::from_chars(
		begin, end, result, std::chars_format::general
	);

	if (error != std::errc{} || next != end ||
		!std::isfinite(result)) {
		DebugError(L"JSON number cannot be represented as a finite float");
		return false;
	}

	// Commit the parsed value only after successful validation.
	_position = static_cast<int>(position);
	value = result;

	return true;
}

std::string JsonReader::readString() {
	skipWhitespace();

	if (_position >= _text.size() || _text[_position] != '"') {
		DebugError(L"Oczekiwano cudzyslowu");
		return "";
	}
		

	++_position; // pomijamy otwierający cudzysłów

	std::string result;

	while (_position < _text.size()) {
		char c = _text[_position++];

		if (c == '"')
			return result;

		if (c == '\\') {
			if (_position >= _text.size()) {
				DebugError(L"Niepelna sekwencja escape");
				return "";
			}
				

			c = _text[_position++];

			switch (c) {
			case '"':  result += '"';  break;
			case '\\': result += '\\'; break;
			case '/':  result += '/';  break;
			case 'b':  result += '\b'; break;
			case 'f':  result += '\f'; break;
			case 'n':  result += '\n'; break;
			case 'r':  result += '\r'; break;
			case 't':  result += '\t'; break;
			case 'u':
				throw std::runtime_error("Escape Unicode jeszcze nieobslugiwany");
			default:
				throw std::runtime_error("Nieprawidlowa sekwencja escape");
			}
		}
		else {
			if (static_cast<unsigned char>(c) < 0x20) {
				DebugError(L"Niedozwolony znak w napisie JSON");
				return "";
			}
				

			result += c;
		}
	}

	DebugError(L"Brak zamykajacego cudzyslowu");
	return "";
}

std::wstring JsonReader::readWstring() {

	std::string s = readString();
	std::wstring w = std::wstring(s.begin(), s.end());
	return w;

}