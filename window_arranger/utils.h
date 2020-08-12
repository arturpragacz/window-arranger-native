#pragma once

#include <fstream>
#include <sstream>


using WindowHandle = HWND;
#define DEFAULTGROUP "CA9422711AE1A81C"


inline void logg(const std::string& s) {
	static std::ofstream f("log.txt"); //TODO better logging
	f << s << std::endl;
	f.flush();
}

inline int error(const std::string& s, int e = 0) {
	logg("ERROR: " + s);
	return e;
}


inline WindowHandle convertCStringToWindowHandle(const char* str) {
	return reinterpret_cast<WindowHandle>(strtoll(str, NULL, 0));
}

inline std::string convertWindowHandleToString(WindowHandle wh) {
	std::stringstream ss;
	ss << std::hex << std::showbase << reinterpret_cast<uint64_t>(wh);

	return ss.str();
}