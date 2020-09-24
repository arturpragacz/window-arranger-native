#pragma once

#include "pch.h"
#include "TaskbarManager.h"

#include <type_traits>
#include <thread>

#define EXCEPTION_STRING CLASSNAME + "::" + __func__


class Messenger {
private:
	static const std::string CLASSNAME;
	std::thread reader;
	TaskbarManager& tbm;
	const WindowGroupFactory& wgf;

public:
	template<typename FR, typename FE>
	Messenger(TaskbarManager& tbm, const WindowGroupFactory& wgf, FR funcRead, FE funcEnd);
	~Messenger();

	int processMessage(const std::string& data);
	void processTimer();

	struct Exception { std::string str; };

private:
	void postMessage(const std::string& message);
	void postMessage(const rapidjson::Value& value);


	void fillOwnMessage(rapidjson::Document& d, int id, std::string_view status);
	void postOwnMessage(int id, std::string_view status = "OK");
	template<typename T>
	void postOwnMessage(int id, std::string_view status, T value);
	template<typename T, typename = std::enable_if_t<!std::is_convertible_v<T, std::string_view>>>
	void postOwnMessage(int id, T value);


	void fillResponse(rapidjson::Document& d, int id, std::string_view status);
	void postResponse(int id, std::string_view status = "OK");
	template<typename T>
	void postResponse(int id, std::string_view status, T value);
	template<typename T, typename = std::enable_if_t<!std::is_convertible_v<T, std::string_view>>>
	void postResponse(int id, T value);
};

template<typename FR, typename FE>
inline Messenger::Messenger(TaskbarManager& tbm, const WindowGroupFactory& wgf, FR funcRead, FE funcEnd) : tbm(tbm), wgf(wgf), reader([&] {
	uint32_t size = 0;
	while (std::cin.read(reinterpret_cast<char*>(&size), sizeof size)) {
		auto data = new std::string(size, '\0');

		if (std::cin.read(&(*data)[0], size)) {
			if (!funcRead(data)) {
				break;
			}
		}
	}
	funcEnd();
}) {}

inline Messenger::~Messenger() {
	reader.join();
}
