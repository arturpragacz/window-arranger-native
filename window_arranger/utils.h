#pragma once


// TODO: better logging
inline void logg(const std::string& s) {
	std::cerr << s << std::endl;
}

inline int error(const std::string& s, int e = 0) {
	logg("ERROR: " + s);
	return e;
}


template <typename T>
class reverse {
private:
	T& iterable;
public:
	explicit reverse(T& iterable) : iterable(iterable) {}
	auto begin() const { return std::rbegin(iterable); }
	auto end() const { return std::rend(iterable); }
};
