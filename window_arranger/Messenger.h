#pragma once

#include "pch.h"
#include "TaskbarManager.h"

#include <type_traits>
#include <thread>

#define EXCEPTION_STRING CLASSNAME + "::" + __func__


class Messenger {
private:
	static const std::string CLASSNAME;
	TaskbarManager& tbm;
	const WindowGroupFactory& wgf;

public:
	Messenger(TaskbarManager& tbm, const WindowGroupFactory& wgf) : tbm(tbm), wgf(wgf) {};

	template<typename FR, typename FE>
	static void readMessage(FR funcRead, FE funcEnd);
	int processMessage(const std::string& data) const;
	void processTimer() const;

	struct Exception { std::string str; };

private:

	void postMessage(const std::string& message) const;
	void postMessage(const rapidjson::Value& value) const;


	void fillOwnMessage(rapidjson::Document& d, int id, std::string_view status) const;
	void postOwnMessage(int id, std::string_view status = "OK") const;
	template<typename T>
	void postOwnMessage(int id, std::string_view status, T value) const;
	template<typename T, typename = std::enable_if_t<!std::is_convertible_v<T, std::string_view>>>
	void postOwnMessage(int id, T value) const;


	void fillResponse(rapidjson::Document& d, int id, std::string_view status) const;
	void postResponse(int id, std::string_view status = "OK") const;
	template<typename T>
	void postResponse(int id, std::string_view status, T value) const;
	template<typename T, typename = std::enable_if_t<!std::is_convertible_v<T, std::string_view>>>
	void postResponse(int id, T value) const;
};

template<typename FR, typename FE>
static void Messenger::readMessage(FR funcRead, FE funcEnd) {
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
}
