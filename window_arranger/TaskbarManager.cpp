#include "pch.h"
#include "TaskbarManager.h"
#include "utils.h"

#include <list>


const std::string TaskbarManager::CLASSNAME = "TaskbarManager";


TaskbarManager::SetArrangementResult TaskbarManager::setArrangement(const Arrangement& destination) {
	auto organizedDestinationWindows = destination.windows.organizeByGroup();
	auto destinationGroups = destination.groups.transformToVector();

	ArrangementGroups::PosGroupVector notSetGroups;
	ArrangementWindows::PosWindowVector notSetWindows;

	try {
		setArrangementWindowGroups(destination);
		auto lock = shi.scoped_lock();
		notSetGroups = setArrangementGroups(destinationGroups);
		notSetWindows = setArrangementWindows(organizedDestinationWindows);
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}

	auto arrangement = updateArrangement();

	return { arrangement, notSetGroups, notSetWindows };
}

void TaskbarManager::setArrangementWindowGroups(const Arrangement& destination) {
	try {
		auto destinationWindows = destination.windows;
		while (true) {
			auto current = getArrangement(
				[&destinationWindows](HWND handle) {
					return destinationWindows.find(handle) != destinationWindows.end();
				},
				false
			);
			if (!current)
				break;

			for (const auto& [handle, position] : current.windows) {
				auto it = destinationWindows.find(handle);
				if (position.getGroup() == it->second.getGroup())
					destinationWindows.erase(it);
			}
			if (destinationWindows.empty())
				break;

			for (const auto& [handle, position] : destinationWindows) {
				if (position.getGroup().isDefault())
					shi.resetWindowAppId(handle);
				else
					shi.setWindowAppId(handle, position.getGroup().getName());
			}

			shi.sleep(250);
		}
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
}

struct TaskbarManager::ForEachGroup {
	ShellIntegrator& shi;
	ForEachGroup(ShellIntegrator& shi) : shi(shi) {}
	template<typename T>
	void operator()(const ShellIntegrator::TaskbarInfo& ti, T callback) { shi.forEachGroup(ti, callback); }
	using CallbackArg = ShellIntegrator::ButtonGroupInfo;
};

// zwraca te, które nie zostały ustawione (bo nie istnieją lub się powtarzają)
ArrangementGroups::PosGroupVector TaskbarManager::setArrangementGroups(ArrangementGroups::PosGroupVector& destinationGroups) {
	try {
		auto ti = shi.getMainTaskbar();

		//using PosGroup = std::pair<WindowGroup, const GroupPosition*>;
		using PosGroup = ArrangementGroups::PosGroup;

		return setArrangementT(
			destinationGroups,
			[] { return GroupPosition(); },
			[&](const ShellIntegrator::ButtonGroupInfo& bgi, const GroupPosition* tmpPos) { return PosGroup(wgf.build(bgi.appId), tmpPos); },
			ti,
			ForEachGroup(shi),
			&ShellIntegrator::moveGroupInTaskbar
		);
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
}

struct TaskbarManager::ForEachInGroup {
	ShellIntegrator& shi;
	ForEachInGroup(ShellIntegrator& shi) : shi(shi) {}
	template<typename T>
	void operator()(const ShellIntegrator::ButtonGroupInfo& bgi, T callback) { shi.forEachInGroup(bgi, callback); }
	using CallbackArg = ShellIntegrator::ButtonInfo;
};

// zwraca te, które nie zostały ustawione (bo nie istnieją lub się powtarzają lub ich nie obserwujemy)
ArrangementWindows::PosWindowVector TaskbarManager::setArrangementWindows(ArrangementWindows::OrganizedPosWindowVectors& organizedDestinationWindows) {
	auto groupsLeft = organizedDestinationWindows.size();

	try {
		//using PosWindow = std::pair<WindowHandle, const Position*>;
		using PosWindow = ArrangementWindows::PosWindow;
		std::vector<PosWindow> notSetVector;

		shi.forEachGroup(shi.getMainTaskbar(),
			[this, &organizedDestinationWindows, &groupsLeft, &notSetVector](const ShellIntegrator::ButtonGroupInfo& bgi) {
				auto organizedDestinationIt = organizedDestinationWindows.find(wgf.build(bgi.appId));
				if (organizedDestinationIt != organizedDestinationWindows.end()) {
					groupsLeft -= 1;

					//using PosWindowVector = std::vector<std::pair<WindowHandle, const Position*>>;
					auto& posWindowVector = organizedDestinationIt->second;

					// usuwamy te, których nie obserwujemy (i później zwracamy je w rezultacie funkcji)
					{
						auto notSetBeginIt = std::partition(posWindowVector.begin(), posWindowVector.end(),
							[&](PosWindow p) {
								return observed.windows.find(p.first) != observed.windows.end();
							}
						);
						notSetVector.insert(notSetVector.end(), std::make_move_iterator(notSetBeginIt), std::make_move_iterator(posWindowVector.end()));
						posWindowVector.erase(notSetBeginIt, posWindowVector.end());
					}

					auto notSetPartVector = setArrangementT(
						posWindowVector,
						[&] { return Position(wgf); },
						[](const ShellIntegrator::ButtonInfo& bi, const Position* tmpPos) { return PosWindow(bi.windowHandle, tmpPos); },
						bgi,
						ForEachInGroup(shi),
						&ShellIntegrator::moveButtonInGroup
					);
					notSetVector.insert(notSetVector.end(), std::make_move_iterator(notSetPartVector.begin()), std::make_move_iterator(notSetPartVector.end()));
				}

				if (groupsLeft > 0)
					return true;
				else
					return false;
			}
		);

		return notSetVector;
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
}

// zwraca te, które nie zostały ustawione (bo nie istnieją lub się powtarzają)
template<typename T1, typename Pos, typename PosC, typename TC, typename S1, typename S2, typename S3>
std::vector<std::pair<T1, const Pos*>> TaskbarManager::setArrangementT(std::vector<std::pair<T1, const Pos*>>& tVector, PosC PosCreator, TC tCreator, const S1& parentInfo, S2 forEachFunc, S3 moveFunc) {
	using T = std::pair<T1, const Pos*>;

	// obecna kolejność (rosnąco)
	std::list<int> order;
	// pomocnicza struktura, łącząca ze sobą pozycje obecne i docelowe
	struct OrderedT {
		T* t;
		std::list<int>::iterator current;
		OrderedT(T* t, std::list<int>::iterator c)
			: t(t), current(c) {}
		OrderedT(T* t)
			: t(t) {}
	};

	std::vector<OrderedT> orderedTs;
	std::transform(tVector.begin(), tVector.end(),
		std::back_inserter(orderedTs), [&order](T& t) {
			return OrderedT(&t, order.end());
		}
	);

	// sortujemy <orderedTs> po <t->first>
	std::sort(orderedTs.begin(), orderedTs.end(),
		[](OrderedT ot1, OrderedT ot2) {
			return ot1.t->first < ot2.t->first;
		}
	);
	const auto tCount = orderedTs.size();

	// przestawiamy interesujące elementy na górę, jednocześnie wypełniając <order> i pola <current> w <orderedTs>
	{
		auto tLeft = tCount;
		int top = 0;
		forEachFunc(parentInfo, [&](const S2::CallbackArg& childInfo) {
			Pos tmpPos = PosCreator();
			T tmpT = tCreator(childInfo, &tmpPos);
			auto [ot, ot2] = std::equal_range(orderedTs.begin(), orderedTs.end(), OrderedT(&tmpT),
				[](const OrderedT& ot1, const OrderedT& ot2) -> bool {
					return ot1.t->first < ot2.t->first;
				}
			);

			if (ot != ot2) {
				tLeft -= 1;
				// przestawiamy element na przód (na górę)
				if (childInfo.index != top) {
					(shi.*moveFunc)(parentInfo, childInfo.index, top);
				}
				ot->current = order.insert(order.end(), top);
				top += 1;
			}

			if (tLeft > 0)
				return true;
			else
				return false;
		});
	}

	// usuwamy te, których obiekty nie istnieją lub które się powtarzają (i później zwracamy je jako rezultat funkcji)
	std::vector<T> notSetVector;
	{
		auto notSetBeginIt = std::partition(orderedTs.begin(), orderedTs.end(),
			[&](const OrderedT& ot) {
				return ot.current != order.end();
			}
		);
		std::transform(notSetBeginIt, orderedTs.end(),
			std::back_inserter(notSetVector), [](const OrderedT& orderedT) {
				return *orderedT.t;
			}
		);
		orderedTs.erase(notSetBeginIt, orderedTs.end());
	}
	
	std::vector<Pos> changedDestinations;
	changedDestinations.resize(orderedTs.size(), PosCreator());

	{
		// obsługujemy ujemne indeksy
		int i = 0;
		for (auto& orderedT : orderedTs) {
			int destinationIndex = orderedT.t->second->getIndex();
			if (destinationIndex < 0) {
				destinationIndex = parentInfo.count + destinationIndex;
				Pos newPos = *orderedT.t->second;
				newPos.update(destinationIndex);
				changedDestinations[i] = std::move(newPos);
				orderedT.t->second = &changedDestinations[i];
			}

			i += 1;
		}
	}

	// sortujemy po docelowym <index> malejąco
	std::sort(orderedTs.begin(), orderedTs.end(),
		[](const OrderedT& ot1, const OrderedT& ot2) {
			return ot1.t->second->getIndex() > ot2.t->second->getIndex();
		}
	);

	{
		// normalizujemy indeksy (gdy są za duże)
		int i = 0;
		int maxDestinationIndex = parentInfo.count - 1;
		for (auto& orderedT : orderedTs) {
			int destinationIndex = orderedT.t->second->getIndex();
			if (destinationIndex > maxDestinationIndex) {
				destinationIndex = maxDestinationIndex;
				Pos newPos = *orderedT.t->second;
				newPos.update(destinationIndex);
				changedDestinations[i] = std::move(newPos);
				orderedT.t->second = &changedDestinations[i];
			}

			i += 1;
			maxDestinationIndex = destinationIndex - 1;
		}
	}
	
	{
		// normalizujemy indeksy (gdy są za małe)
		int i = 0;
		int minDestinationIndex = 0;
		for (auto& orderedT : reverse(orderedTs)) {
			int destinationIndex = orderedT.t->second->getIndex();
			if (destinationIndex < minDestinationIndex) {
				destinationIndex = minDestinationIndex;
				Pos newPos = *orderedT.t->second;
				newPos.update(destinationIndex);
				changedDestinations[i] = std::move(newPos);
				orderedT.t->second = &changedDestinations[i];
			}

			i += 1;
			minDestinationIndex = destinationIndex + 1;
		}
	}

	// przestawiamy obiekty, uaktualniając na bieżąco ich pozycję w <order>
	// robimy to w kolejności: od obiektu, który ma być najdalej (najniżej), do obiektu, który ma być najbliżej (najwyżej)
	for (const auto& orderedT : orderedTs) {
		int currentIndex = *orderedT.current;
		int destinationIndex = orderedT.t->second->getIndex();
		assert(currentIndex <= destinationIndex);
		(shi.*moveFunc)(parentInfo, currentIndex, destinationIndex);

		auto begin = order.erase(orderedT.current);
		auto end = order.end();
		for (auto indexIt = begin; indexIt != end; ++indexIt) {
			*indexIt -= 1;
		}
	}

	return notSetVector;
}

Arrangement TaskbarManager::updateArrangement() {
	auto changed = Arrangement();
	try {
		auto lock = shi.scoped_lock();
		auto ti = shi.getMainTaskbar();

		shi.forEach(ti, [this, &changed](const ShellIntegrator::ButtonInfo& bi) {
			auto it = observed.windows.find(bi.windowHandle);
			if (it != observed.windows.end()) {
				auto& position = it->second;
				auto group = wgf.build(bi.group.appId);
				if (position.update(group, bi.index)) {
					changed.windows.insert(std::make_pair(bi.windowHandle, position));
					auto groupPosition = GroupPosition(bi.group.index);
					observed.groups.insert(std::make_pair(group, groupPosition));
					changed.groups.insert(std::make_pair(group, groupPosition));
				}
			}
			return true;
		});

		auto neededGroups = std::unordered_set<WindowGroup>();
		std::transform(observed.windows.begin(), observed.windows.end(),
			std::inserter(neededGroups, neededGroups.end()), [](const ArrangementWindows::value_type& winPos) {
				const auto& [_, pos] = winPos;
				return pos.getGroup();
			});

		for (auto it = observed.groups.begin(); it != observed.groups.end(); ) {
			if (neededGroups.find(it->first) == neededGroups.end()) {
				it = observed.groups.erase(it);
			}
			else {
				++it;
			}
		}

		shi.forEachGroup(ti, [this, &changed](const ShellIntegrator::ButtonGroupInfo& bgi) {
			auto group = wgf.build(bgi.appId);
			auto it = observed.groups.find(group);
			if (it != observed.groups.end()) {
				auto& groupPosition = it->second;
				if (groupPosition.update(bgi.index))
					changed.groups.insert_or_assign(group, groupPosition);
			}
			return true;
		});
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}
	return changed;
}
