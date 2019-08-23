#include "pch.h"
#include "TTLibWrapper.h"

const std::string TTLibWrapper::CLASSNAME = "TTLibWrapper";

TTLibWrapper::TTLibWrapper() {
	DWORD dwError = TTLib_Init();
	if (dwError == TTLIB_OK) {
		dwError = TTLib_LoadIntoExplorer();
		if (dwError != TTLIB_OK) {
			throw Exception{ EXCEPTION_STRING + " TTLib_LoadIntoExplorer: " + to_string(dwError) };
		}
	}
	else {
		throw Exception{ EXCEPTION_STRING + " TTLib_Init: " + to_string(dwError) };
	}
}

TTLibWrapper::~TTLibWrapper() {
	TTLib_UnloadFromExplorer();
	TTLib_Uninit();
}
