#pragma once

#include "WindowGroup.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>

#include <map>
#include <vector>
#include <algorithm>
#include <sstream>


using WindowHandle = HWND;

inline HWND convertCStringToWindowHandle(const char* str) {
	return reinterpret_cast<HWND>(strtoll(str, NULL, 0));
}

inline std::string convertWindowHandleToString(HWND wh) {
	std::stringstream ss;
	ss << std::hex << std::showbase << reinterpret_cast<uint64_t>(wh);

	return ss.str();
}


class Position {
private:
	int index;
	WindowGroup group;

public:
	explicit Position(const WindowGroupFactory& wgf) : group(wgf.build()), index(-1) {}
	Position(int index, const WindowGroupFactory& wgf) : group(wgf.build()), index(index) {}
	Position(const WindowGroup& group, int index) : group(group), index(index) {}
	Position(const rapidjson::Value& value, const WindowGroupFactory& wgf)
		: group(wgf.build(value["group"])), index(value["index"].GetInt()) {}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kObjectType);
		value.AddMember("group", group.toJson(allocator), allocator);
		value.AddMember("index", index, allocator);
		return value;
	}

	int getIndex() const {
		return index;
	}
	const WindowGroup& getGroup() const {
		return group;
	}
	bool update(const WindowGroup& newGroup, int newIndex) {
		bool updated = false;
		updated = update(newGroup) || updated;
		updated = update(newIndex) || updated;
		return updated;
	}
	bool update(const WindowGroup& newGroup) {
		return group.update(newGroup);
	}
	bool update(int newIndex) {
		bool updated = false;
		if (this->index != newIndex) {
			this->index = newIndex;
			updated = true;
		}
		return updated;
	}
};


class Arrangement : public std::map<WindowHandle, Position> {
public:
	Arrangement() = default;
	Arrangement(const rapidjson::Value& value, const WindowGroupFactory& wgf) {
		for (const auto& posWindowJson : value.GetArray()) {
			this->insert(
				std::pair<WindowHandle, Position>(
					convertCStringToWindowHandle(posWindowJson["handle"].GetString()),
					Position(posWindowJson["position"], wgf)
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
			value.PushBack(
				rapidjson::Value(rapidjson::kObjectType)
					.AddMember("handle", convertWindowHandleToString(posWindow.first), allocator)
					.AddMember("position", posWindow.second.toJson(allocator), allocator),
				allocator
			);
		}
		return value;
	}

	auto organizeByGroup() const {
		using PosWindowVector = std::vector<std::pair<map::key_type, const map::mapped_type*>>;
		std::map<WindowGroup, PosWindowVector> organized;
		for (auto& posWindow : *this) {
			auto organizedWindowGroupIt = organized.find(posWindow.second.getGroup());

			if (organizedWindowGroupIt != organized.end()) {
				auto& vectorOfPosWindows = organizedWindowGroupIt->second;
				vectorOfPosWindows.emplace_back(posWindow.first, &posWindow.second);
			}
			else {
				organized.insert(std::pair(posWindow.second.getGroup(), PosWindowVector{ std::pair(posWindow.first, &posWindow.second) }));
			}
		}

		return organized;
	}

	auto organizeByGroupAndWindowHandle() const {
		typedef std::map<map::key_type, const map::mapped_type*> PosWindowMap;
		std::map<WindowGroup, PosWindowMap> organized;
		for (const auto& posWindow : *this) {
			auto organizedWindowGroupIt = organized.find(posWindow.second.getGroup());

			if (organizedWindowGroupIt != organized.end()) {
				auto& mapOfPosWindows = organizedWindowGroupIt->second;
				mapOfPosWindows.insert(std::pair(posWindow.first, &posWindow.second));
			}
			else {
				organized.insert(std::pair(posWindow.second.getGroup(), PosWindowMap{ std::pair(posWindow.first, &posWindow.second) }));
			}
		}
		return organized;
	}
};
