#include "pch.h"
#include "TaskbarManager.h"
#include "utils.h"

#include <list>


const std::string TaskbarManager::CLASSNAME = "TaskbarManager";


Arrangement TaskbarManager::setArrangement(const Arrangement& destination) {
	auto organizedDestinationWindows = destination.windows.organizeByGroup();

	try {
		setArrangementWindowGroups(organizedDestinationWindows);
		auto lock = shi.scoped_lock();
		setArrangementGroups(destination.groups);
		setArrangementWindows(organizedDestinationWindows);
	}
	catch (const ShellIntegrator::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}

	return updateArrangement();
}

void TaskbarManager::setArrangementWindowGroups(const ArrangementWindows::OrganizedPosWindowVectors& organizedDestinationWindows) {
	try {
		for (const auto&[windowGroup, posWindowVector] : organizedDestinationWindows) {
			// przenosimy do właściwej grupy
			for (const auto& posWindow : posWindowVector) {
				if (posWindow.second->getGroup().isDefault())
					shi.resetWindowAppId(posWindow.first);
				else
					shi.setWindowAppId(posWindow.first, posWindow.second->getGroup().getName());
			}
		}

		// czekamy, aż zmiany rozpropagują się do graficznej powłoki
		shi.sleep(500);
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

void TaskbarManager::setArrangementGroups(const ArrangementGroups& destinationGroups) {
	try {
		auto ti = shi.getMainTaskbar();

		//using PosGroupVector = std::vector<std::pair<WindowGroup, const GroupPosition*>>;
		auto posGroupVector = destinationGroups.transformToVector();
		//using PosGroup = std::pair<WindowGroup, const GroupPosition*>;
		using PosGroup = std::remove_reference_t<decltype(posGroupVector)>::value_type;

		setArrangementT(
			posGroupVector,
			[] { return GroupPosition(); },
			[&](const ShellIntegrator::ButtonGroupInfo& bgi, const GroupPosition* tmpPos) { return PosGroup(wgf.build(bgi.appId), tmpPos); },
			ti,
			ForEachGroup(shi),
			&ShellIntegrator::moveGroupInTaskbar);
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

void TaskbarManager::setArrangementWindows(ArrangementWindows::OrganizedPosWindowVectors& organizedDestinationWindows) {
	auto groupsLeft = organizedDestinationWindows.size();

	try {
		shi.forEachGroup(shi.getMainTaskbar(),
			[this, &organizedDestinationWindows, &groupsLeft](const ShellIntegrator::ButtonGroupInfo& bgi) {
			auto organizedDestinationIt = organizedDestinationWindows.find(wgf.build(bgi.appId));
			if (organizedDestinationIt != organizedDestinationWindows.end()) {
				groupsLeft -= 1;

				//using PosWindowVector = std::vector<std::pair<WindowHandle, const Position*>>;
				auto& posWindowVector = organizedDestinationIt->second;
				//using PosWindow = std::pair<WindowHandle, const Position*>;
				using PosWindow = std::remove_reference_t<decltype(posWindowVector)>::value_type;

				// zmieniamy tylko te, które już obserwujemy
				posWindowVector.erase(
					std::remove_if(posWindowVector.begin(), posWindowVector.end(),
						[&](PosWindow p) {
							return observed.windows.find(p.first) == observed.windows.end();
						}
					),
					posWindowVector.end()
				);

				setArrangementT(
					posWindowVector,
					[&] { return Position(wgf); },
					[](const ShellIntegrator::ButtonInfo& bi, const Position* tmpPos) { return PosWindow(bi.windowHandle, tmpPos); },
					bgi,
					ForEachInGroup(shi),
					&ShellIntegrator::moveButtonInGroup);
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
}

template<typename T1, typename Pos, typename PosC, typename TC, typename S1, typename S2, typename S3>
void TaskbarManager::setArrangementT(std::vector<std::pair<T1, const Pos*>>& tVector, PosC PosCreator, TC tCreator, const S1& parentInfo, S2 forEachFunc, S3 moveFunc) {
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
		});

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

	// usuwamy te, których obiekty nie istnieją (lub które się powtarzają)
	orderedTs.erase(
		std::remove_if(orderedTs.begin(), orderedTs.end(),
			[&](const OrderedT& ot) {
				return ot.current == order.end();
			}
		),
		orderedTs.end()
	);
	
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
		});

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
