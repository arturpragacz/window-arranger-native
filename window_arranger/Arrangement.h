#pragma once

#include "utils.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>

#include <map>
#include <vector>
#include <algorithm>

class WindowGroup {
private:
	bool defGroup;
	std::string name;

public:
	WindowGroup() : defGroup(true) {}
	explicit WindowGroup(std::string_view nm) {
		if (nm == DEFAULTGROUP) {
			defGroup = true;
		}
		else {
			defGroup = false;
			name = nm;
		}
	}
	explicit WindowGroup(const rapidjson::Value& value) : defGroup(value["defGroup"].GetBool()) {
		if (!defGroup)
			name = value["name"].GetString();
	}

	bool operator<(const WindowGroup& rhs) const {
		if (defGroup != rhs.defGroup)
			return defGroup;
		else
			return name < rhs.name;
	}

	std::string_view getName() const {
		if (name == DEFAULTGROUP)
			return DEFAULTGROUP;
		else
			return name;
	}
	bool isDefault() const {
		return defGroup;
	}
	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kObjectType);
		value.AddMember("defGroup", defGroup, allocator);
		if (!defGroup)
			value.AddMember("name", name, allocator);
		return value;
	}

	bool update(const WindowGroup& newGroup) {
		bool updated = false;

		if (this->defGroup != newGroup.defGroup) {
			this->defGroup = newGroup.defGroup;
			updated = true;
		}

		if (this->defGroup) {
			this->name.clear();
		}
		else {
			if (this->name != newGroup.name) {
				this->name = name;
				updated = true;
			}
		}

		return updated;
	}
};

class Position {
private:
	int index;
	WindowGroup group;

public:
	Position() : group(), index(-1) {}
	explicit Position(int index) : group(), index(index) {}
	Position(const WindowGroup& group, int index) : group(group), index(index) {}
	explicit Position(const rapidjson::Value& value)
		: group(value["group"]), index(value["index"].GetInt()) {}

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
	Arrangement(const rapidjson::Value& value) {
		for (const auto& posWindowJson : value.GetArray()) {
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
		typedef std::vector<const map::value_type*> PosWindowVector;
		std::map<WindowGroup, PosWindowVector> organized;
		for (const auto& posWindow : *this) {
			auto organizedWindowGroupIt = organized.find(posWindow.second.getGroup());

			if (organizedWindowGroupIt != organized.end()) {
				auto& vectorOfPosWindows = organizedWindowGroupIt->second;
				vectorOfPosWindows.push_back(&posWindow);
			}
			else {
				organized.insert(std::pair(posWindow.second.getGroup(), std::vector<decltype(&posWindow)> { &posWindow }));
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
