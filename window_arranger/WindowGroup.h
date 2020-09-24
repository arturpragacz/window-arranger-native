#pragma once

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>


class WindowGroup {
	friend class WindowGroupFactory;
private:
	bool defGroup;
	std::string name;
	const std::string& defGroupName;

	explicit WindowGroup(const std::string& defGroupName) : defGroupName(defGroupName), defGroup(true) {}
	WindowGroup(std::string_view name, const std::string& defGroupName) : defGroupName(defGroupName) {
		if (name == defGroupName) {
			this->defGroup = true;
		}
		else {
			this->defGroup = false;
			this->name = name;
		}
	}
	WindowGroup(const rapidjson::Value& value, const std::string& defGroupName) : defGroupName(defGroupName), defGroup(value["defGroup"].GetBool()) {
		if (!this->defGroup)
			this->name = value["name"].GetString();
		if (name == defGroupName) {
			defGroup = true;
			name.clear();
		}
	}

	void normalizeBasedOnName() noexcept {
		defGroup = name == defGroupName;
		if (defGroup)
			name.clear();
	}
	bool equalDefGroupNames(const WindowGroup& other) const noexcept {
		return &defGroupName == &other.defGroupName || defGroupName == other.defGroupName;
	}

public:
	WindowGroup(const WindowGroup&) = default;
	WindowGroup(WindowGroup&&) = default;
	WindowGroup& operator=(const WindowGroup& other) {
		name = other.name;

		if (equalDefGroupNames(other))
			defGroup = other.defGroup;
		else
			normalizeBasedOnName();

		return *this;
	}
	WindowGroup& operator=(WindowGroup&& other) noexcept {
		swap(*this, other);

		return *this;
	}
	friend void swap(WindowGroup& first, WindowGroup& second) noexcept {
		using std::swap;

		swap(first.name, second.name);

		if (first.equalDefGroupNames(second)) {
			swap(first.defGroup, second.defGroup);
		}
		else {
			first.normalizeBasedOnName();
			second.normalizeBasedOnName();
		}
	}

	bool operator<(const WindowGroup& rhs) const noexcept {
		const std::string& rhsGroupName = rhs.defGroup ? rhs.defGroupName : rhs.name;
		const std::string& thisGroupName = defGroup ? defGroupName : name;

		return thisGroupName < rhsGroupName;
	}

	const std::string& getName() const noexcept {
		if (defGroup)
			return defGroupName;
		else
			return name;
	}
	bool isDefault() const noexcept {
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
		const WindowGroup& newGroupNorm = equalDefGroupNames(newGroup) ?
			newGroup : WindowGroup(newGroup.defGroup ? newGroup.defGroupName : newGroup.name, defGroupName);

		bool updated = false;

		if (this->defGroup != newGroupNorm.defGroup) {
			this->defGroup = newGroupNorm.defGroup;
			updated = true;
		}

		if (this->defGroup) {
			this->name.clear();
		}
		else {
			if (this->name != newGroupNorm.name) {
				this->name = newGroupNorm.name;
				updated = true;
			}
		}

		return updated;
	}
};

class WindowGroupFactory {
private:
	const std::string defGroupName;

public:
	WindowGroupFactory(std::string_view defGroupName) : defGroupName(defGroupName) {}

	WindowGroup build() const {
		return WindowGroup(defGroupName);
	}
	WindowGroup build(std::string_view name) const {
		return WindowGroup(name, defGroupName);
	}
	WindowGroup build(const rapidjson::Value& value) const {
		return WindowGroup(value, defGroupName);
	}
};
