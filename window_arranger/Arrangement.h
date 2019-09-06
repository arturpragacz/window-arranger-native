#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>

#include <map>
#include <vector>
#include <algorithm>

using WindowHandle = HWND;

class Position {
public:
	friend class Arrangement;
	friend class TaskbarManager; //TODO: better solution to the problem of accessing private variables (getters)
	Position() : hasDefaultAppId(true), index(-1) {};
	Position(int index) : hasDefaultAppId(true), index(index) {}
	Position(const std::string& appId, int index) : hasDefaultAppId(false), appId(appId), index(index) {}
	Position(const rapidjson::Value& value)
		: hasDefaultAppId(value["inDefaultGroup"].GetBool()), index(value["index"].GetInt()) {
		if (!hasDefaultAppId)
			appId = value["group"].GetString();
	}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kObjectType);
		value.AddMember("inDefaultGroup", hasDefaultAppId, allocator);
		if (!this->hasDefaultAppId)
			value.AddMember("group", appId, allocator);
		value.AddMember("index", index, allocator);
		return value;
	}
	bool update(bool newAppIdDefault, const std::string& newAppId, int newIndex) {
		bool updated = false;

		if (this->hasDefaultAppId != newAppIdDefault) {
			this->hasDefaultAppId = newAppIdDefault;
			updated = true;
		}

		if (this->hasDefaultAppId) {
			this->appId.clear();
		}
		else {
			if (this->appId != newAppId) {
				this->appId = newAppId;
				updated = true;
			}
		}

		if (this->index != newIndex) {
			this->index = newIndex;
			updated = true;
		}
		return updated;
	}
	bool update(const std::string& newAppId, int newIndex) {
		return update(false, newAppId, newIndex);
	}
	bool update(int newIndex) {
		bool updated = false;
		if (this->index != newIndex) {
			this->index = newIndex;
			updated = true;
		}
		return updated;
	}
	//inline bool operator==(const Position& p) const {
	//	return this->wAppId == p.wAppId && this->index == p.index;
	//}
	//inline bool operator!=(const Position& p) {
	//	return !(*this == p);
	//}
	//bool isSameGroup(const Position& p) const {
	//	return appId == p.appId;
	//}
	//bool isSameIndex(const Position& p) const {
	//	return index == p.index;
	//}

private:
	bool hasDefaultAppId;
	std::string appId;
	int index;
};

class Arrangement : public std::map<WindowHandle, Position> {
public:
	Arrangement() = default;
	Arrangement(const rapidjson::Value& value) {
		for (const auto& posWindowJson : value.GetArray()) {
			WindowHandle convertCStringToWindowHandle(const char* str); // deklaracja funkcji z window_arranger.cpp
			this->insert(
				std::pair<WindowHandle, Position>(
					convertCStringToWindowHandle(posWindowJson["handle"].GetString()),
					Position(posWindowJson["position"])
				)
			);
		}
	}

	operator bool() const {
		return !map::empty();
	}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kArrayType);
		for (const auto& posWindow : *this) {
			std::string convertWindowHandleToString(WindowHandle wh); // deklaracja funkcji z window_arranger.cpp
			value.PushBack(
				rapidjson::Value(rapidjson::kObjectType)
					.AddMember("handle", convertWindowHandleToString(posWindow.first), allocator)
					.AddMember("position", posWindow.second.toJson(allocator), allocator),
				allocator
			);
		}
		return value;
	}

	//auto organizeByGroupVectorSortedByWindowHandle() const {
	auto organizeByGroupVector() const {
		typedef std::vector<const map::value_type*> WindowGroupArray;
		std::map<std::string, WindowGroupArray> organized;
		for (const auto& posWindow : *this) {
			auto organizedWindowGroupIt = organized.find(posWindow.second.appId);

			if (organizedWindowGroupIt != organized.end()) {
				auto& vectorOfPosWindows = organizedWindowGroupIt->second;
				vectorOfPosWindows.push_back(&posWindow);
			}
			else {
				organized.insert(std::pair(posWindow.second.appId, std::vector<decltype(&posWindow)> { &posWindow }));
			}
		}

		//for (auto& windowGroup : organized) {
		//	auto& vectorOfPosWindows = windowGroup.second;
		//	std::sort(vectorOfPosWindows.begin(), vectorOfPosWindows.end(),
		//		[](const map::value_type* posWindow1Ptr, const map::value_type* posWindow2Ptr) {
		//			return posWindow1Ptr->first < posWindow2Ptr->first;
		//		}
		//	);
		//}

		return organized;
	}

	auto organizeByGroup() const {
		typedef std::map<map::key_type, const map::mapped_type*> WindowGroup;
		std::map<std::string, WindowGroup> organized;
		for (const auto& posWindow : *this) {
			auto organizedWindowGroupIt = organized.find(posWindow.second.appId);

			if (organizedWindowGroupIt != organized.end()) {
				auto& mapOfPosWindows = organizedWindowGroupIt->second;
				mapOfPosWindows.insert(std::pair(posWindow.first, &posWindow.second));
			}
			else {
				organized.insert(std::pair(posWindow.second.appId, WindowGroup { std::pair(posWindow.first, &posWindow.second) }));
			}
		}
		return organized;
	}
};
