#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>

#include <map>

using Handle = HANDLE;

class Position {
public:
	std::wstring wAppId;
	std::string appId;
	int index;

	Position() : index(-1) {};
	Position(const std::wstring& wAppId, int index) : wAppId(wAppId), index(index) {
		convertToUTF8();
	}
	Position(const rapidjson::Value& value)
		: appId(value["group"].GetString()), index(value["index"].GetInt()) {
		convertToUTF16();
	}

	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) const {
		return std::move(rapidjson::Value(rapidjson::kObjectType)
			.AddMember("group", appId, allocator)
			.AddMember("index", index, allocator));
	}
	template <class CharT>
	bool update(const std::basic_string<CharT>& newAppId, int newIndex) {
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

private:
	bool updateAppId(const std::string& newAppId) {
		if (this->appId != newAppId) {
			this->appId = newAppId;
			convertToUTF16();
			return true;
		}
		return false;
	}
	bool updateAppId(const std::wstring& newAppId) {
		if (this->wAppId != newAppId) {
			this->wAppId = newAppId;
			convertToUTF8();
			return true;
		}
		return false;
	}
	void convertToUTF8() {
		int oldLength = static_cast<int>(wAppId.length());
		char* buffer = new char[oldLength * 2];
		int size = WideCharToMultiByte(CP_UTF8, 0, wAppId.c_str(), oldLength, buffer, oldLength * 2, NULL, NULL);
		appId.assign(buffer, size);
		delete[] buffer;
	}
	void convertToUTF16() {
		int oldLength = static_cast<int>(appId.length());
		wchar_t* buffer = new wchar_t[oldLength];
		int size = MultiByteToWideChar(CP_UTF8, 0, appId.c_str(), oldLength, buffer, oldLength);
		wAppId.assign(buffer, size);
		delete[] buffer;
	}

};

class Arrangement : public std::map<Handle, Position> {

public:
	Arrangement() = default;
	Arrangement(const rapidjson::Value& value) {
		for (const auto& posWindowJson : value.GetArray()) {
			this->insert(
				std::pair<Handle, Position>(
					reinterpret_cast<Handle>(posWindowJson["handle"].GetInt64()),
					Position(posWindowJson["position"])
				)
			);
		}
	}
	rapidjson::Value toJson(rapidjson::MemoryPoolAllocator<>& allocator) {
		rapidjson::Value value(rapidjson::kArrayType);
		for (const auto& posWindow : *this) {
			value.PushBack(
				rapidjson::Value(rapidjson::kObjectType)
					.AddMember("handle", reinterpret_cast<int64_t>(posWindow.first), allocator)
					.AddMember("position", posWindow.second.toJson(allocator), allocator),
				allocator
			);
		}
		return value;
	}

private:
	const std::string CLASSNAME = "Arrangement";
};
