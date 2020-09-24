#pragma once


// TODO: better logging
inline void logg(const std::string& s) {
	std::cerr << s << std::endl;
}

inline int error(const std::string& s, int e = 0) {
	logg("ERROR: " + s);
	return e;
}
