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
	WindowGroup group;
	int index;

public:
	explicit Position(const WindowGroupFactory& wgf) : group(wgf.build()), index(-1) {}
	Position(int index, const WindowGroupFactory& wgf) : group(wgf.build()), index(index) {}
	Position(WindowGroup group, int index) : group(std::move(group)), index(index) {}
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

class GroupPosition {
private:
	int index;
public:
	GroupPosition() : index(-1) {}
	explicit GroupPosition(int index) : index(index) {}
	GroupPosition(const rapidjson::Value& value) : index(value["index"].GetInt()) {}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kObjectType);
		value.AddMember("index", index, allocator);
		return value;
	}

	int getIndex() const {
		return index;
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


class ArrangementWindows : public std::map<WindowHandle, Position> {
public:
	ArrangementWindows() = default;
	ArrangementWindows(const rapidjson::Value& value, const WindowGroupFactory& wgf) : ArrangementWindows() {
		for (const auto& posWindowJson : value.GetArray()) {
			this->insert(
				std::pair<WindowHandle, Position>(
					convertCStringToWindowHandle(posWindowJson["handle"].GetString()),
					Position(posWindowJson["position"], wgf)
					)
			);
		}
	}

	using PosWindowVector = std::vector<std::pair<map::key_type, const map::mapped_type*>>;
	using OrganizedPosWindowVectors = std::map<WindowGroup, PosWindowVector>;

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

	OrganizedPosWindowVectors organizeByGroup() const {
		OrganizedPosWindowVectors organized;
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
		using PosWindowMap = std::map<map::key_type, const map::mapped_type*>;
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

class ArrangementGroups : public std::map<WindowGroup, GroupPosition> {
public:
	ArrangementGroups() = default;
	ArrangementGroups(const rapidjson::Value& value, const WindowGroupFactory& wgf) : ArrangementGroups() {
		for (const auto& posGroupJson : value.GetArray()) {
			this->insert(
				std::pair<WindowGroup, GroupPosition>(
					wgf.build(posGroupJson[0]),
					GroupPosition(posGroupJson[1])
					)
			);
		}
	}

	using PosGroupVector = std::vector<std::pair<map::key_type, const map::mapped_type*>>;

	PosGroupVector transformToVector() const {
		PosGroupVector transformed;
		for (auto& posGroup : *this) {
			transformed.emplace_back(posGroup.first, &posGroup.second);
		}
		return transformed;
	}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kArrayType);
		for (const auto& posGroup : *this) {
			value.PushBack(
				rapidjson::Value(rapidjson::kArrayType)
				.PushBack(posGroup.first.toJson(allocator), allocator)
				.PushBack(posGroup.second.toJson(allocator), allocator),
				allocator
			);
		}
		return value;
	}
};

class Arrangement {
public:
	ArrangementWindows windows;
	ArrangementGroups groups;

	Arrangement() = default;
	Arrangement(ArrangementWindows windows, ArrangementGroups groups) : windows(std::move(windows)), groups(std::move(groups)) {}
	Arrangement(const rapidjson::Value& value, const WindowGroupFactory& wgf) : windows(value["windows"], wgf), groups(value["groups"], wgf) {}

	operator bool() const {
		return !windows.empty() || !groups.empty();
	}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kObjectType);
		value.AddMember("windows", windows.toJson(allocator), allocator);
		value.AddMember("groups", groups.toJson(allocator), allocator);
		return value;
	}
};
