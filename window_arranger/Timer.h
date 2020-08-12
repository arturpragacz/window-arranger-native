#pragma once

class Timer {
	UINT_PTR timerId;
public:
	Timer(UINT elapse) {
		timerId = SetTimer(NULL, NULL, elapse, NULL);
		if (timerId == 0) {
			throw Exception();
		}
	}
	~Timer() { KillTimer(NULL, timerId); }
	struct Exception {};
};

