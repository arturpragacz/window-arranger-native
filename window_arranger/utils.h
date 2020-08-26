#pragma once

#include <fstream>


// TODO: better logging
inline void logg(const std::string& s) {
	static std::ofstream f("log.txt");
	f << s << std::endl;
	f.flush();
}

inline int error(const std::string& s, int e = 0) {
	logg("ERROR: " + s);
	return e;
}
