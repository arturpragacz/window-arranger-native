#pragma once

#include "pch.h"
#include "TTLibWrapper.h"
#include "windows_utils.h"

#include <functional>

using std::to_string;
#define EXCEPTION_STRING CLASSNAME + "::" + __func__


class ShellIntegrator {
private:
	static const std::string CLASSNAME;

	TTLibWrapper ttlib; // can throw

	bool locked = false;
	struct Lock {
		ShellIntegrator& shi;
		Lock(ShellIntegrator& shi) : shi(shi) { shi.lock(); }
		~Lock() { shi.unlock(); }
	};

public:
	struct ButtonGroupInfo {
		HANDLE taskbar;
		int index;
		HANDLE handle;
		std::string appId;
		int buttonCount;
	};
	struct ButtonInfo {
		const ButtonGroupInfo& group;
		int index;
		HANDLE handle;
		HWND windowHandle;

		ButtonInfo(const ButtonGroupInfo& g) : group(g) {}
	};

	ShellIntegrator();
	~ShellIntegrator();

	template<typename T> void forEachGroup(T callback) const;
	template<typename T> bool forEachInGroup(const ShellIntegrator::ButtonGroupInfo& bgi, T callback) const;
	template<typename T> void forEach(T callback) const;

	void lock();
	void unlock();
	[[nodiscard]] Lock scoped_lock() { return Lock(*this); }

	DWORD getParentProcessId();
	std::string getProcessAppId(DWORD processId);
	void setWindowAppId(HWND hWnd, std::string_view appId) const;
	void resetWindowAppId(HWND hWnd) const;
	void moveButtonInGroup(const ButtonGroupInfo& bgi, int indexFrom, int indexTo);
	void sleep(int msecs);

	struct Exception { std::string str; };
};


//void (*callback)(const ShellIntegrator::ButtonGroupInfo&))
template<typename T>
void ShellIntegrator::forEachGroup(T callback) const {
	ShellIntegrator::ButtonGroupInfo bgi;
	int buttonGroupCount;

	try {
		bgi.taskbar = ttlib.getMainTaskbar();
		buttonGroupCount = ttlib.getButtonGroupCount(bgi.taskbar);
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}

	try {
		for (int i = 0; i < buttonGroupCount; ++i) {
			bgi.index = i;
			bgi.handle = ttlib.getButtonGroup(bgi.taskbar, i);

			std::wstring tmpAppId = ttlib.getButtonGroupAppId(bgi.handle);
			bgi.appId = convertToUTF8(tmpAppId);

			bgi.buttonCount = ttlib.getButtonCount(bgi.handle);

			if (!callback(bgi))
				break;
		}
	}
	catch (const UTFConversionException& e) {
		throw Exception{ EXCEPTION_STRING + ": " + to_string(bgi.index) + " | " + e.str };
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + ": " + to_string(bgi.index) + " | " + e.str };
	}
}

//void (*callback)(const ShellIntegrator::ButtonInfo&))
template<typename T>
bool ShellIntegrator::forEachInGroup(const ShellIntegrator::ButtonGroupInfo& bgi, T callback) const {
	ShellIntegrator::ButtonInfo bi(bgi);
	int buttonCount = bgi.buttonCount;

	try {
		for (int i = 0; i < buttonCount; ++i) {
			bi.index = i;
			bi.handle = ttlib.getButton(bgi.handle, i);

			try {
				bi.windowHandle = ttlib.getButtonWindow(bi.handle);

				if (!callback(bi))
					break;
			}
			catch (const TTLibWrapper::Exception&) {}
		}
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + ": " + to_string(bgi.index) + " " + to_string(bi.index) + " | " + e.str };
	}

	return true;
}

//void (*callback)(const ShellIntegrator::ButtonInfo&))
template<typename T>
void ShellIntegrator::forEach(T callback) const {
	using namespace std::placeholders;
	forEachGroup(std::bind(&ShellIntegrator::forEachInGroup<T>, this, _1, callback));
}
