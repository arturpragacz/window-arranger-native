#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>

#include <map>
#include <vector>
#include <algorithm>

using Handle = HANDLE;

class Position {
public:
	friend class Arrangement;
	Position() : index(-1) {};
	Position(const std::string& appId, int index) : appId(appId), index(index) {}
	Position(const rapidjson::Value& value)
		: appId(value["group"].GetString()), index(value["index"].GetInt()) {}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		return std::move(rapidjson::Value(rapidjson::kObjectType)
			.AddMember("group", appId, allocator)
			.AddMember("index", index, allocator));
	}
	bool update(const std::string& newAppId, int newIndex) {
		bool updated = false;
		if (updateAppId(newAppId)) {
			updated = true;
		}
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
	std::string appId;
	int index;
	bool updateAppId(const std::string& newAppId) {
		if (this->appId != newAppId) {
			this->appId = newAppId;
			//convertToUTF16(); TODOs
			return true;
		}
		return false;
	}
};

class Arrangement : public std::map<Handle, Position> {
public:
	Arrangement() = default;
	Arrangement(const rapidjson::Value& value) {
		for (const auto& posWindowJson : value.GetArray()) {
			this->insert(
				std::pair<Handle, Position>(
					reinterpret_cast<Handle>(posWindowJson["handle"].GetInt64()), // TODO: cast from string to int64
					Position(posWindowJson["position"])
				)
			);
		}
	}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		rapidjson::Value value(rapidjson::kArrayType);
		for (const auto& posWindow : *this) {
			value.PushBack(
				rapidjson::Value(rapidjson::kObjectType)
					.AddMember("handle", reinterpret_cast<int64_t>(posWindow.first), allocator) // TODO: cast from int64 to string "0x<num>"
					.AddMember("position", posWindow.second.toJson(allocator), allocator),
				allocator
			);
		}
		return value;
	}
	auto organize() const {
		std::map<std::string, std::vector<const map::value_type*>> organized;
		for (const auto& posWindow : *this) {
			auto intraWindowGroupIt = organized.find(posWindow.second.appId);

			if (intraWindowGroupIt != organized.end()) {
				auto& vectorOfPosWindows = intraWindowGroupIt->second;
				vectorOfPosWindows.push_back(&posWindow);
			}
			else {
				organized.insert(std::pair(posWindow.second.appId, std::vector<decltype(&posWindow)> { &posWindow }));
			}
		}

		for (auto& windowGroup : organized) {
			auto& vectorOfPosWindows = windowGroup.second;
			std::sort(vectorOfPosWindows.begin(), vectorOfPosWindows.end(),
				[](const map::value_type* posWindow1Ptr, const map::value_type* posWindow2Ptr) {
					return posWindow1Ptr->second.index < posWindow2Ptr->second.index;
				}
			);
		}

		return organized;
	}
};
