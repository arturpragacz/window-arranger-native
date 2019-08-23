#include "pch.h"
#include "TaskbarManager.h"

#include <TTLib.h>


const std::string TaskbarManager::CLASSNAME = "TaskbarManager";
//using std::to_string;


TaskbarManager::TaskbarManager() try {
}
catch (const TTLibWrapper::Exception& e) {
	throw Exception{ EXCEPTION_STRING + " | " + e.str };
}

TaskbarManager::~TaskbarManager() {
}


Arrangement TaskbarManager::addToObserved(const std::set<Handle>& handleSet) {
	Arrangement arrangement;
	try {
		ttl.forEach([this, &handleSet, &arrangement](const TTLibWrapper::ButtonInfo& bi) {
			if (handleSet.find(bi.buttonWindow) != handleSet.end()) {
				auto positionIt = observed.find(bi.buttonWindow);
				if (positionIt == observed.end()) {
					std::pair<Handle, Position> pair(bi.buttonWindow, Position(bi.appId, bi.buttonIndex));
					observed.insert(pair);
					arrangement.insert(pair);
				}
			}
		});
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return arrangement;
}

void TaskbarManager::deleteFromObserved(const std::vector<Handle>& handles) {
	for (const auto& handle : handles) {
		observed.erase(handle);
	}
}

Arrangement TaskbarManager::getArrangement() {
	return getArrangement(true, nullptr);
}

Arrangement TaskbarManager::getArrangement(const std::set<Handle>& handleSet) {
	return getArrangement(false, &handleSet);
}

Arrangement TaskbarManager::getArrangement(bool all, const std::set<Handle>* handleSetPtr) {
	Arrangement arrangement;
	try {
		ttl.forEach([this, all, handleSetPtr, &arrangement](const TTLibWrapper::ButtonInfo& bi) {
			if (all || handleSetPtr->find(bi.buttonWindow) != handleSetPtr->end()) {
				auto positionIt = observed.find(bi.buttonWindow);
				if (positionIt != observed.end()) {
					Position& position = positionIt->second;
					position.update(bi.appId, bi.buttonIndex);
					arrangement.insert(std::pair<Handle, Position>(bi.buttonWindow, position));
				}
			}
		});
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return arrangement;
}

//TODO
int TaskbarManager::setArrangement(const Arrangement& arrangement) {
	//ttl.forEach([this, &arrangement](const TTLibWrapper::ButtonInfo& bi) {
	//	if (handleSet.find(bi.buttonWindow) != handleSet.end()) {
	//		auto positionIt = observed.find(bi.buttonWindow);
	//		if (positionIt == observed.end()) {
	//			std::pair<Handle, Position> pair(bi.buttonWindow, Position(bi.appId, bi.buttonIndex));
	//			observed.insert(pair);
	//			arrangement.insert(pair);
	//		}
	//	}
	//});
	return 0;
}

//TODO
int TaskbarManager::updateObserved() {
	return 0;
}