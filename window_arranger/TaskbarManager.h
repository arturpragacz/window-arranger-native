#pragma once

#include "pch.h"
#include "Arrangement.h"
#include "TTLibWrapper.h"

#include <vector>
#include <set>

#define EXCEPTION_STRING CLASSNAME + "::" + __func__

class TaskbarManager {

private:
	static const std::string CLASSNAME;
	Arrangement observed;
	TTLibWrapper ttl;

public:
	TaskbarManager();
	~TaskbarManager();

	Arrangement addToObserved(const std::set<Handle>& handleSet);
	void deleteFromObserved(const std::vector<Handle>& handles);
	Arrangement getArrangement();
	Arrangement getArrangement(const std::set<Handle>& handleSet);
	int setArrangement(const Arrangement& arrangement);
	int updateObserved();

	struct Exception { std::string str; };

private:
	Arrangement getArrangement(bool all, const std::set<Handle>* handleSetPtr);
};
