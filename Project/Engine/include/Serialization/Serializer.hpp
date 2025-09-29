#pragma once
#include <string>

class Serializer {
public:
	static void SerializeScene(const std::string& scenePath);
	static void DeserializeScene(const std::string& scenePath);

private:
	Serializer() = delete;
	~Serializer() = delete;
};