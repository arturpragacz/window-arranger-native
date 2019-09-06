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
	//~TaskbarManager();

	Arrangement addToObserved(const std::set<WindowHandle>& handleSet);
	void deleteFromObserved(const std::vector<WindowHandle>& handles);
	Arrangement getArrangement();
	Arrangement getArrangement(const std::set<WindowHandle>& handleSet);
	Arrangement setArrangement(const Arrangement& arrangement);
	Arrangement updateArrangement();

	struct Exception { std::string str; };

private:
	Arrangement getArrangement(bool all, const std::set<WindowHandle>* handleSetPtr);
};
