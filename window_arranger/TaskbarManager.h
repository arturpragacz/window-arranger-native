#pragma once

#include "pch.h"
#include "Arrangement.h"
#include "ShellIntegrator.h"

#include <vector>
#include <unordered_set>

#define EXCEPTION_STRING CLASSNAME + "::" + __func__


class TaskbarManager {
private:
	static const std::string CLASSNAME;
	Arrangement observed;
	ShellIntegrator& shi;
	const WindowGroupFactory& wgf;

public:
	TaskbarManager(ShellIntegrator& shi, const WindowGroupFactory& wgf) : shi(shi), wgf(wgf) {}

	template<typename InputIt>
	Arrangement addToObserved(InputIt first, InputIt last);
	template<typename InputIt>
	void deleteFromObserved(InputIt first, InputIt last);
	Arrangement getArrangement();
	template<typename InputIt>
	Arrangement getArrangement(InputIt first, InputIt last, bool inObserved = true);
	Arrangement setArrangement(const Arrangement& arrangement);
	Arrangement updateArrangement();

	struct Exception { std::string str; };

private:
	template<typename Pred>
	Arrangement getArrangement(Pred isGoodHandle, bool inObserved);
};


template<typename InputIt>
Arrangement TaskbarManager::addToObserved(InputIt first, InputIt last) {
	std::unordered_set<WindowHandle> handles(first, last);
	Arrangement arrangement;
	try {
		int windowsLeft = static_cast<int>(handles.size());
		auto lock = shi.scoped_lock();
		shi.forEach([this, &handles, &arrangement, &windowsLeft](const ShellIntegrator::ButtonInfo& bi) {
			if (handles.find(bi.windowHandle) != handles.end()) {
				windowsLeft -= 1;
				auto positionIt = observed.find(bi.windowHandle);
				if (positionIt == observed.end()) {
					std::pair<WindowHandle, Position> pair(bi.windowHandle, Position(wgf.build(bi.group.appId), bi.index));
					observed.insert(pair);
					arrangement.insert(pair);
				}
			}

			if (windowsLeft > 0)
				return true;
			else
				return false;
		});
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return arrangement;
}

template<typename InputIt>
void TaskbarManager::deleteFromObserved(InputIt first, InputIt last) {
	for (auto handle = first; handle != last; ++handle) {
		observed.erase(*handle);
	}
}

inline Arrangement TaskbarManager::getArrangement() {
	return getArrangement([](const auto& handle) -> bool { return true; }, true);
}

template<typename InputIt>
Arrangement TaskbarManager::getArrangement(InputIt first, InputIt last, bool inObserved) {
	std::unordered_set<WindowHandle> handles(first, last);
	return getArrangement([&handles](const auto& handle) -> bool { return handles.find(handle) != handles.end(); }, inObserved);
}

template<typename Pred>
Arrangement TaskbarManager::getArrangement(Pred isGoodHandle, bool inObserved) {
	Arrangement arrangement;
	try {
		auto lock = shi.scoped_lock();
		shi.forEach([this, &isGoodHandle, inObserved, &arrangement](const ShellIntegrator::ButtonInfo& bi) {
			if (isGoodHandle(bi.windowHandle)) {
				if (inObserved) {
					auto positionIt = observed.find(bi.windowHandle);
					if (positionIt != observed.end()) {
						Position& position = positionIt->second;
						position.update(wgf.build(bi.group.appId), bi.index);
						arrangement.insert(std::pair<WindowHandle, Position>(bi.windowHandle, position));
					}
				}
				else {
					Position position(wgf.build(bi.group.appId), bi.index);
					arrangement.insert(std::pair<WindowHandle, Position>(bi.windowHandle, position));
				}
			}
			return true;
		});
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return arrangement;
}