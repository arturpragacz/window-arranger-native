#include "pch.h"
#include "TaskbarManager.h"

#include <list>


const std::string TaskbarManager::CLASSNAME = "TaskbarManager";


Arrangement TaskbarManager::setArrangement(const Arrangement& destination) {
	auto organizedDestination = destination.organizeByGroup();
	int groupsLeft = static_cast<int>(organizedDestination.size());

	try {
		for (const auto& [windowGroup, posWindowVector] : organizedDestination) {
			// przenosimy do właściwej grupy
			for (const auto& posWindow : posWindowVector) {
				if (posWindow->second.getGroup().isDefault())
					shi.resetWindowAppId(posWindow->first);
				else
					shi.setWindowAppId(posWindow->first, posWindow->second.getGroup().getName());
			}
		}

		// czekamy, aż zmiany rozpropagują się do graficznej powłoki
		shi.sleep(500);

		auto lock = shi.scoped_lock();
		shi.forEachGroup(
			[this, &organizedDestination, &groupsLeft](const ShellIntegrator::ButtonGroupInfo& bgi) {
				auto organizedDestinationIt = organizedDestination.find(wgf.build(bgi.appId));
				if (organizedDestinationIt != organizedDestination.end()) {
					groupsLeft -= 1;

					// typedef std::vector<const std::map<WindowHandle, Position>::value_type*> PosWindowVector;
					// typedef std::vector<const std::pair<const WindowHandle, Position>*> PosWindowVector;
					auto& posWindowVector = organizedDestinationIt->second;
					// typedef const std::pair<const WindowHandle, Position> PosWindow;
					using PosWindow = std::remove_pointer_t<std::remove_reference_t<decltype(posWindowVector)>::value_type>;

					// zmieniamy tylko te, które już obserwujemy
					posWindowVector.erase(
						std::remove_if(posWindowVector.begin(), posWindowVector.end(),
							[&](PosWindow* p) {
								return observed.find(p->first) == observed.end();
							}
						),
						posWindowVector.end()
					);
					const int windowsNumber = static_cast<int>(posWindowVector.size());

					// sortujemy po [WindowHandle]
					std::sort(posWindowVector.begin(), posWindowVector.end(),
						[](PosWindow* posWindow1Ptr, PosWindow* posWindow2Ptr) {
							return posWindow1Ptr->first < posWindow2Ptr->first;
						}
					);
					
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
					std::transform(posWindowVector.begin(), posWindowVector.end(),
						std::back_inserter(orderedPosWindows), [&order](PosWindow* d) {
							return OrderedPosWindow(d, order.end());
					});
					// przestawiamy interesujące okna na górę, jednocześnie wypełniając [order] i pola [current] w [orderedPosWindows]
					{
						int windowsLeft = windowsNumber;
						int top = 0;
						shi.forEachInGroup(bgi, [&](const ShellIntegrator::ButtonInfo& bi) {
							PosWindow tmpDestination = PosWindow(bi.windowHandle, Position(wgf));
							auto [opw, opw2] = std::equal_range(orderedPosWindows.begin(), orderedPosWindows.end(),
								OrderedPosWindow(&tmpDestination),
								[](const OrderedPosWindow& opw1, const OrderedPosWindow& opw2) -> bool {
								return opw1.destination->first < opw2.destination->first;
							});

							if (opw != opw2) {
								windowsLeft -= 1;
								// przestawiamy okno na górę
								if (bi.index != top) {
									shi.moveButtonInGroup(bgi, bi.index, top);
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
						shi.moveButtonInGroup(bgi, currentIndex, destinationIndex);

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
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}

	return updateArrangement();
}

Arrangement TaskbarManager::updateArrangement() {
	Arrangement changed;
	try {
		auto lock = shi.scoped_lock();
		shi.forEach([this, &changed](const ShellIntegrator::ButtonInfo& bi) {
			auto positionIt = observed.find(bi.windowHandle);
			if (positionIt != observed.end()) {
				Position& position = positionIt->second;
				if (position.update(wgf.build(bi.group.appId), bi.index))
					changed.insert_or_assign(bi.windowHandle, position);
			}
			
			return true;
		});
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return changed;
}