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
	void setArrangementWindowGroups(const ArrangementWindows::OrganizedArrangementWindows& organizedDestinationWindows);
	void setArrangementGroups(const ArrangementGroups& destinationGroups);
	void setArrangementWindows(ArrangementWindows::OrganizedArrangementWindows& organizedDestinationWindows);
	template<typename T1, typename Pos, typename PosC, typename TC, typename S1, typename S2, typename S3>
	void setArrangementT(const std::vector<std::pair<T1, const Pos*>>& tVector, PosC PosCreator, TC tCreator, const S1& parentInfo, S2 forEachFunc, S3 moveFunc);
	struct ForEachGroup;
	struct ForEachInGroup;
};


template<typename InputIt>
Arrangement TaskbarManager::addToObserved(InputIt first, InputIt last) {
	auto handles = std::unordered_set<WindowHandle>(first, last);
	auto arrangement = Arrangement();
	try {
		auto windowsLeft = handles.size();
		auto lock = shi.scoped_lock();
		shi.forEach(shi.getMainTaskbar(), [this, &handles, &arrangement, &windowsLeft](const ShellIntegrator::ButtonInfo& bi) {
			if (handles.find(bi.windowHandle) != handles.end()) {
				windowsLeft -= 1;
				auto positionIt = observed.windows.find(bi.windowHandle);
				if (positionIt == observed.windows.end()) {
					auto group = wgf.build(bi.group.appId);
					auto windowPair = std::make_pair(bi.windowHandle, Position(group, bi.index));
					observed.windows.insert(windowPair);
					arrangement.windows.insert(windowPair);
					auto groupPair = std::make_pair(group, GroupPosition(bi.group.index));
					observed.groups.insert(groupPair);
					arrangement.groups.insert(groupPair);
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
	// niepotrzebne groupy są regularnie usuwane podczas <updateArrangement>
	//auto groupsWithRemovedWindows = std::unordered_set<WindowGroup>();
	for (auto handle = first; handle != last; ++handle) {
		auto it = observed.windows.find(*handle);
		if (it != observed.windows.end()) {
			//groupsWithRemovedWindows.insert(it->second.getGroup());
			observed.windows.erase(it);
		}
	}
	//for (const auto& posWindow : observed.windows) {
	//	groupsWithRemovedWindows.erase(posWindow.second.getGroup());
	//}
	//for (const auto& group : groupsWithRemovedWindows) {
	//	observed.groups.erase(group);
	//}
}

inline Arrangement TaskbarManager::getArrangement() {
	return getArrangement([](const auto& handle) -> bool { return true; }, true);
}

template<typename InputIt>
Arrangement TaskbarManager::getArrangement(InputIt first, InputIt last, bool inObserved) {
	auto handles = std::unordered_set<WindowHandle>(first, last);
	return getArrangement([&handles](const auto& handle) -> bool { return handles.find(handle) != handles.end(); }, inObserved);
}

template<typename Pred>
Arrangement TaskbarManager::getArrangement(Pred isGoodHandle, bool inObserved) {
	auto arrangement = Arrangement();
	try {
		auto lock = shi.scoped_lock();
		shi.forEach(shi.getMainTaskbar(), [this, &isGoodHandle, inObserved, &arrangement](const ShellIntegrator::ButtonInfo& bi) {
			if (isGoodHandle(bi.windowHandle)) {
				auto group = wgf.build(bi.group.appId);
				auto groupPosition = GroupPosition(bi.group.index);
				if (inObserved) {
					auto positionIt = observed.windows.find(bi.windowHandle);
					if (positionIt != observed.windows.end()) {
						auto& position = positionIt->second;
						position.update(group, bi.index);
						observed.groups.insert_or_assign(group, groupPosition);
						arrangement.windows.insert(std::make_pair(bi.windowHandle, position));
						arrangement.groups.insert(std::make_pair(group, groupPosition));
					}
				}
				else {
					auto position = Position(group, bi.index);
					arrangement.windows.insert(std::make_pair(bi.windowHandle, position));
					arrangement.groups.insert(std::make_pair(group, groupPosition));
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
