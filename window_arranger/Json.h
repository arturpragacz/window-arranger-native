#pragma once

#include "TaskbarManager.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>

inline rapidjson::Value toJson(const TaskbarManager::SetArrangementResult& setResult, rapidjson::MemoryPoolAllocator<>& allocator) {
	rapidjson::Value value(rapidjson::kObjectType);

	value.AddMember("arrangement", setResult.arrangement.toJson(allocator), allocator);

	rapidjson::Value notSet(rapidjson::kObjectType);

	rapidjson::Value notSetGroups(rapidjson::kArrayType);
	for (const auto& posGroup : setResult.notSetGroups) {
		notSetGroups.PushBack(
			rapidjson::Value(rapidjson::kArrayType)
				.PushBack(posGroup.first.toJson(allocator), allocator)
				.PushBack(posGroup.second->toJson(allocator), allocator),
			allocator
		);
	}
	notSet.AddMember("groups", notSetGroups, allocator);

	rapidjson::Value notSetWindows(rapidjson::kArrayType);
	for (const auto& posWindow : setResult.notSetWindows) {
		notSetWindows.PushBack(
			rapidjson::Value(rapidjson::kObjectType)
				.AddMember("handle", convertWindowHandleToString(posWindow.first), allocator)
				.AddMember("position", posWindow.second->toJson(allocator), allocator),
			allocator
		);
	}
	notSet.AddMember("windows", notSetWindows, allocator);

	value.AddMember("notSet", notSet, allocator);
	
	return value;
}

template<typename T>
rapidjson::Value toJson(const T& value, rapidjson::MemoryPoolAllocator<>& allocator) {
	return value.toJson(allocator);
}
