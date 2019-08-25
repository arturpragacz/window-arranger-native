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
					std::pair<Handle, Position> pair(bi.buttonWindow, Position(bi.group.appId, bi.buttonIndex));
					observed.insert(pair);
					arrangement.insert(pair);
				}
			}
			return true; //TODO: better conditional
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
					position.update(bi.group.appId, bi.buttonIndex);
					arrangement.insert(std::pair<Handle, Position>(bi.buttonWindow, position));
				}
			}
			return true; //TODO: better conditional
		});
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return arrangement;
}

//TODO: przenoszenie tak¿e miêdzy appId
//Arrangement TaskbarManager::setArrangement(const Arrangement& destination) {
//	Arrangement changed;
//	auto organizedDestination = destination.organize();
//	try {
//		ttl.forEach(
//			[this, &changed, &organizedDestination](const TTLibWrapper::ButtonGroupInfo& bgi) {
//				auto intraWindowGroupIt = organizedDestination.find(bgi.appId);
//				if (intraWindowGroupIt != organizedDestination.end()) {
//					auto& vectorOfPosWindows = intraWindowGroupIt->second;
//
//
//					return true;
//				}
//				else
//					return false;
//			},
//			[this, &organizedDestination](const TTLibWrapper::ButtonInfo& bi) {
//			auto arrPositionIt = destination.find(bi.buttonWindow);
//			if (arrPositionIt == destination.end())
//				return;
//			auto obsPositionIt = observed.find(bi.buttonWindow);
//			if (obsPositionIt == observed.end())
//				return;
//
//			//if (bi.appId != (*arrPositionIt).second.wAppId)
//			auto destinationIndex = (*arrPositionIt).second.index;
//			if (bi.buttonIndex != destinationIndex)
//				ttl.buttonMoveInButtonGroup(bi.buttonGroup, bi.buttonIndex, destinationIndex);
//		});
//	}
//	catch (const TTLibWrapper::Exception& e) {
//		throw Exception{ EXCEPTION_STRING + " | " + e.str };
//	}
//	return 0;
//}

//TODO
int TaskbarManager::updateObserved() {
	return 0;
}