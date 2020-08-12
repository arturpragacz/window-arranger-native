#include "pch.h"
#include "TaskbarManager.h"

#include <list>


const std::string TaskbarManager::CLASSNAME = "TaskbarManager";


TaskbarManager::TaskbarManager() try {
}
catch (const TTLibWrapper::Exception& e) {
	throw Exception{ EXCEPTION_STRING + " | " + e.str };
}


Arrangement TaskbarManager::addToObserved(const std::set<WindowHandle>& handleSet) {
	Arrangement arrangement;
	try {
		int windowsLeft = static_cast<int>(handleSet.size());
		auto lock = ttl.scoped_lock();
		ttl.forEach([this, &handleSet, &arrangement, &windowsLeft](const TTLibWrapper::ButtonInfo& bi) {
			if (handleSet.find(bi.windowHandle) != handleSet.end()) {
				windowsLeft -= 1;
				auto positionIt = observed.find(bi.windowHandle);
				if (positionIt == observed.end()) {
					std::pair<WindowHandle, Position> pair(bi.windowHandle, Position(WindowGroup(bi.group.appId), bi.index));
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

Arrangement TaskbarManager::getArrangement(const std::set<WindowHandle>& handleSet, bool inObserved) {
	return getArrangement(false, &handleSet);
}

Arrangement TaskbarManager::getArrangement(bool all, const std::set<WindowHandle>* handleSetPtr, bool inObserved) {
	Arrangement arrangement;
	try {
		auto lock = ttl.scoped_lock();
		ttl.forEach([this, all, handleSetPtr, inObserved, &arrangement](const TTLibWrapper::ButtonInfo& bi) {
			if (all || handleSetPtr->find(bi.windowHandle) != handleSetPtr->end()) {
				if (inObserved) {
					auto positionIt = observed.find(bi.windowHandle);
					if (positionIt != observed.end()) {
						Position& position = positionIt->second;
						position.update(WindowGroup(bi.group.appId), bi.index);
						arrangement.insert(std::pair<WindowHandle, Position>(bi.windowHandle, position));
					}
				}
				else {
					Position position(WindowGroup(bi.group.appId), bi.index);
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
	auto organizedDestination = destination.organizeByGroup();
	int groupsLeft = static_cast<int>(organizedDestination.size());
	try {
		auto lock = ttl.scoped_lock();
		ttl.forEachGroup(
			[this, &organizedDestination, &groupsLeft](const TTLibWrapper::ButtonGroupInfo& bgi) {
				auto windowGroupIt = organizedDestination.find(WindowGroup(bgi.appId));
				if (windowGroupIt != organizedDestination.end()) {
					groupsLeft -= 1;

					// typedef std::vector<const std::map<WindowHandle, Position>::value_type*> PosWindowVector;
					// typedef std::vector<const std::pair<const WindowHandle, Position>*> PosWindowVector;
					auto windowGroup = windowGroupIt->second;
					// typedef const std::pair<const WindowHandle, Position> PosWindow;
					using PosWindow = std::remove_pointer_t<decltype(windowGroup)::value_type>;

					// zmieniamy tylko te, które już obserwujemy
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
						[](PosWindow* posWindow1Ptr, PosWindow* posWindow2Ptr) {
							return posWindow1Ptr->first < posWindow2Ptr->first;
						}
					);

					// przenosimy do właściwej grupy
					for (const auto& posWindow : windowGroup) {
						if (posWindow->second.getGroup().isDefault())
							ttl.resetWindowAppId(posWindow->first);
						else
							ttl.setWindowAppId(posWindow->first, posWindow->second.getGroup().getName());
					}
					
					// obecna kolejność (rosnąco)
					std::list<int> order;
					// pomocnicza struktura, łącząca ze sobą pozycje okien obecne i docelowe
					struct OrderedPosWindow {
						PosWindow* destination;
						decltype(order)::iterator current;
						OrderedPosWindow(PosWindow* d, decltype(order)::iterator c)
							: destination(d), current(c) {}
						OrderedPosWindow(PosWindow* d)
							: destination(d) {}
					};
					std::vector<OrderedPosWindow> orderedPosWindows;
					std::transform(windowGroup.begin(), windowGroup.end(),
						std::back_inserter(orderedPosWindows), [&order](PosWindow* d) {
							return OrderedPosWindow(d, order.end());
					});
					// przestawiamy interesujące okna na górę, jednocześnie wypełniając [order] i pola [current] w [orderedPosWindows]
					{
						int windowsLeft = windowsNumber;
						int top = 0;
						ttl.forEachInGroup(bgi, [&](const TTLibWrapper::ButtonInfo& bi) {
							PosWindow tmpDestination = PosWindow(bi.windowHandle, Position());
							auto [opw, opw2] = std::equal_range(orderedPosWindows.begin(), orderedPosWindows.end(),
								OrderedPosWindow(&tmpDestination),
								[](const OrderedPosWindow& opw1, const OrderedPosWindow& opw2) -> bool {
								return opw1.destination->first < opw2.destination->first;
							});

							if (opw != opw2) {
								windowsLeft -= 1;
								// przestawiamy okno na górę
								if (bi.index != top) {
									if (!ttl.moveButtonInGroup(bgi, bi.index, top)) {
										throw Exception{ EXCEPTION_STRING + " ttl.moveButtonInGroup: " + to_string(bgi.index)
											+ " " + to_string(bi.index) + " " + to_string(top) };
									}
								}
								opw->current = order.insert(order.end(), top);
								top += 1;
							}

							if (windowsLeft > 0)
								return true;
							else
								return false;
						});
					}

					// usuwamy te, których okna nie istnieją (lub które się powtarzają)
					orderedPosWindows.erase(
						std::remove_if(orderedPosWindows.begin(), orderedPosWindows.end(),
							[&](const decltype(orderedPosWindows)::value_type& opw) {
								return opw.current == order.end();
							}
						),
						orderedPosWindows.end()
					);

					// sortujemy po docelowym [index] malejąco
					std::sort(orderedPosWindows.begin(), orderedPosWindows.end(),
						[](const OrderedPosWindow& opw1, const OrderedPosWindow& opw2) {
							return opw1.destination->second.getIndex() > opw2.destination->second.getIndex();
						}
					);

					// normalizujemy indeksy (gdy są za duże)
					std::vector<std::remove_const_t<PosWindow>> changedDestinations;
					changedDestinations.reserve(orderedPosWindows.size());
					{
						int maxDestinationIndex = bgi.buttonCount - 1;
						int i = 0;
						for (auto& orderedPosWindow : orderedPosWindows) {
							int destinationIndex = orderedPosWindow.destination->second.getIndex();
							if (destinationIndex > maxDestinationIndex) {
								destinationIndex = maxDestinationIndex;
								Position normalizedPos = orderedPosWindow.destination->second;
								normalizedPos.update(destinationIndex);
								changedDestinations.push_back(
									PosWindow(orderedPosWindow.destination->first, std::move(normalizedPos)));
								orderedPosWindow.destination = &changedDestinations[i];
								i += 1;
							}

							maxDestinationIndex = destinationIndex - 1;
						}
					}

					// przestawiamy okna, uaktualniając na bieżąco ich pozycję w [order]
					// robimy to w kolejności: od okna, które ma być najniżej, do okna, które ma być najwyżej
					for (const auto& orderedPosWindow : orderedPosWindows) {
						int currentIndex = *orderedPosWindow.current;
						int destinationIndex = orderedPosWindow.destination->second.getIndex();
						assert(currentIndex <= destinationIndex);
						if (!ttl.moveButtonInGroup(bgi, currentIndex, destinationIndex)) {
							throw Exception{ EXCEPTION_STRING + " ttl.moveButtonInGroup: " + to_string(bgi.index)
								+ " " + to_string(currentIndex) + " " + to_string(destinationIndex) };
						}

						auto begin = order.erase(orderedPosWindow.current);
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
		auto lock = ttl.scoped_lock();
		ttl.forEach([this, &changed](const TTLibWrapper::ButtonInfo& bi) {
			auto positionIt = observed.find(bi.windowHandle);
			if (positionIt != observed.end()) {
				Position& position = positionIt->second;
				if (position.update(WindowGroup(bi.group.appId), bi.index))
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