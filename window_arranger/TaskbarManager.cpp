#include "pch.h"
#include "TaskbarManager.h"

#include <TTLib.h>

//#include <list>

const std::string TaskbarManager::CLASSNAME = "TaskbarManager";
//using std::to_string;


TaskbarManager::TaskbarManager() try {
}
catch (const TTLibWrapper::Exception& e) {
	throw Exception{ EXCEPTION_STRING + " | " + e.str };
}

//TaskbarManager::~TaskbarManager() {
//}


Arrangement TaskbarManager::addToObserved(const std::set<WindowHandle>& handleSet) {
	Arrangement arrangement;
	try {
		int windowsLeft = static_cast<int>(handleSet.size());;
		ttl.forEach([this, &handleSet, &arrangement, &windowsLeft](const TTLibWrapper::ButtonInfo& bi) {
			if (handleSet.find(bi.windowHandle) != handleSet.end()) {
				windowsLeft -= 1;
				auto positionIt = observed.find(bi.windowHandle);
				if (positionIt == observed.end()) {
					std::pair<WindowHandle, Position> pair(bi.windowHandle, Position(bi.group.appId, bi.index));
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
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return arrangement;
}

void TaskbarManager::deleteFromObserved(const std::vector<WindowHandle>& handles) {
	for (const auto& handle : handles) {
		observed.erase(handle);
	}
}

Arrangement TaskbarManager::getArrangement() {
	return getArrangement(true, nullptr);
}

Arrangement TaskbarManager::getArrangement(const std::set<WindowHandle>& handleSet) {
	return getArrangement(false, &handleSet);
}

Arrangement TaskbarManager::getArrangement(bool all, const std::set<WindowHandle>* handleSetPtr) {
	Arrangement arrangement;
	try {
		ttl.forEach([this, all, handleSetPtr, &arrangement](const TTLibWrapper::ButtonInfo& bi) {
			if (all || handleSetPtr->find(bi.windowHandle) != handleSetPtr->end()) {
				// TODO: szukanie te¿ innych (trzeba by podzieliæ na przypadek all i ten drugi)
				auto positionIt = observed.find(bi.windowHandle);
				if (positionIt != observed.end()) {
					Position& position = positionIt->second;
					position.update(bi.group.appId, bi.index);
					arrangement.insert(std::pair<WindowHandle, Position>(bi.windowHandle, position));
				}
			}
			return true;
		});
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return arrangement;
}

Arrangement TaskbarManager::setArrangement(const Arrangement& destination) {
	//Arrangement changed;

	//TODO: przenoszenie tak¿e miêdzy appId
	//for (const auto& posWindow : destination) {
	//	ttl.setWindowAppId(posWindow.first, posWindow.second.appId);
	//}

	auto organizedDestination = destination.organizeByGroupVector();
	int groupsLeft = static_cast<int>(organizedDestination.size());
	try {
		ttl.forEachGroup(
			[this, &organizedDestination, &groupsLeft](const TTLibWrapper::ButtonGroupInfo& bgi) {
				auto windowGroupIt = organizedDestination.find(bgi.appId);
				if (windowGroupIt != organizedDestination.end()) {
					groupsLeft -= 1;

					// typedef std::vector<const std::map<WindowHandle, Position>::value_type*> WindowGroupArray;
					// typedef std::vector<const std::pair<const WindowHandle, Position>*> WindowGroupArray;
					auto windowGroup = windowGroupIt->second;
					// typedef const std::pair<const WindowHandle, Position> DestinationType;
					using DestinationType = std::remove_pointer_t<decltype(windowGroup)::value_type>;
					// bêdziemy zmieniaæ tylko te, które ju¿ obserwujemy
					// TODO: zmiana te¿ innych (trzeba by zrobiæ Arrangement.merge)
					windowGroup.erase(
						std::remove_if(windowGroup.begin(), windowGroup.end(),
							[&](decltype(windowGroup)::value_type p) {
								return observed.find(p->first) == observed.end();
							}
						),
						windowGroup.end()
					);
					const int windowsNumber = static_cast<int>(windowGroup.size());

					// sortujemy po [WindowHandle]
					std::sort(windowGroup.begin(), windowGroup.end(),
						[](DestinationType* posWindow1Ptr, DestinationType* posWindow2Ptr) {
							return posWindow1Ptr->first < posWindow2Ptr->first;
						}
					);
					
					// obecna kolejnoœæ (rosn¹co)
					std::list<int> order;
					// pomocnicza struktura, ³¹cz¹ca ze sob¹ pozycje okien obecne i docelowe
					struct PosWindow {
						DestinationType* destination;
						decltype(order)::iterator current;
						PosWindow(DestinationType* d, decltype(order)::iterator c)
							: destination(d), current(c) {}
						PosWindow(DestinationType* d)
							: destination(d) {}
					};
					std::vector<PosWindow> posWindows;
					std::transform(windowGroup.begin(), windowGroup.end(),
						std::back_inserter(posWindows), [&order](DestinationType* d) {
							return PosWindow(d, order.end());
					});
					{
						int windowsLeft = windowsNumber;
						int top = 0;
						ttl.forEachInGroup(bgi, [&](const TTLibWrapper::ButtonInfo& bi) {
							DestinationType tmpDestination = DestinationType(bi.windowHandle, Position());
							auto[pw, pw2] = std::equal_range(posWindows.begin(), posWindows.end(),
								PosWindow(&tmpDestination),
								[](const PosWindow& pw1, const PosWindow& pw2) -> bool {
								return pw1.destination->first < pw2.destination->first;
							});

							if (pw != pw2) {
								windowsLeft -= 1;
								// przestawiamy okno na górê
								if (bi.index != top) {
									if (!ttl.moveButtonInGroup(bgi, bi.index, top)) {
										throw Exception{ EXCEPTION_STRING + " ttl.moveButtonInGroup: " + to_string(bgi.index)
											+ " " + to_string(bi.index) + " " + to_string(top) };
									}
								}
								pw->current = order.insert(order.end(), top);
								top += 1;
							}

							if (windowsLeft > 0)
								return true;
							else
								return false;
						});
					}

					// sortujemy po docelowym [index] malej¹co
					std::sort(posWindows.begin(), posWindows.end(),
						[](const PosWindow& pw1, const PosWindow& pw2) {
							return pw1.destination->second.index > pw2.destination->second.index;
						}
					);

					// normalizujemy indeksy (gdy s¹ za du¿e)
					std::vector<std::remove_const_t<DestinationType>> changedDestinations;
					changedDestinations.reserve(posWindows.size());
					{
						int maxDestinationIndex = bgi.buttonCount - 1;
						int i = 0;
						for (auto& posWindow : posWindows) {
							int destinationIndex = posWindow.destination->second.index;
							if (destinationIndex > maxDestinationIndex) {
								destinationIndex = maxDestinationIndex;
								Position normalizedPos = posWindow.destination->second;
								normalizedPos.update(destinationIndex);
								changedDestinations.push_back(
									DestinationType(posWindow.destination->first, std::move(normalizedPos)));
								posWindow.destination = &changedDestinations[i];
								i += 1;
							}

							maxDestinationIndex = destinationIndex - 1;
						}
					}

					// przestawiamy okna, uaktualniaj¹c na bie¿¹co ich pozycjê w [order]
					for (const auto& posWindow : posWindows) {
						int currentIndex = *posWindow.current;
						int destinationIndex = posWindow.destination->second.index;
						assert(currentIndex <= destinationIndex);
						if (!ttl.moveButtonInGroup(bgi, currentIndex, destinationIndex)) {
							throw Exception{ EXCEPTION_STRING + " ttl.moveButtonInGroup: " + to_string(bgi.index)
								+ " " + to_string(currentIndex) + " " + to_string(destinationIndex) };
						}

						//auto begin = std::lower_bound(order.begin(), order.end(), currentIndex);
						auto begin = order.erase(posWindow.current);
						auto end = order.end();
						for (auto indexIt = begin; indexIt != end; ++indexIt) {
							*indexIt -= 1;
						}
					}
				}

				if (groupsLeft > 0)
					return true;
				else
					return false;
		});
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return updateArrangement();
}

Arrangement TaskbarManager::updateArrangement() {
	Arrangement changed;
	try {
		ttl.forEach([this, &changed](const TTLibWrapper::ButtonInfo& bi) {
			auto positionIt = observed.find(bi.windowHandle);
			if (positionIt != observed.end()) {
				Position& position = positionIt->second;
				if (position.update(bi.group.appId, bi.index))
					changed.insert(std::pair<WindowHandle, Position>(bi.windowHandle, position));
			}
			
			return true;
		});
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return changed;
}