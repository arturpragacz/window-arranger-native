#pragma once

//#ifndef TTLIBWRAPPER_H
//#define TTLIBWRAPPER_H
//#define TTLIBWRAPPER_H_INSIDE

#include <TTLib.h>

#include <functional>

using std::to_string;
#define EXCEPTION_STRING CLASSNAME + "::" + __func__

class TTLibWrapper
{
private:
	static const std::string CLASSNAME;

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

	TTLibWrapper();
	~TTLibWrapper();

	template <typename T> void forEachGroup(T callback) const;
	template <typename T> bool forEachInGroup(const TTLibWrapper::ButtonGroupInfo& bgi, T callback) const;
	template <typename T> void forEach(T callback) const;

	bool setWindowAppId(HWND hWnd, const std::string& appId) const;
	bool moveButtonInGroup(const ButtonGroupInfo& bgi, int indexFrom, int indexTo);

	struct Exception { std::string str; };

private:
	std::string convertToUTF8(std::wstring wideString) const {
		int oldLength = static_cast<int>(wideString.length());
		char* buffer = new char[oldLength * 2];
		int size = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), oldLength, buffer, oldLength * 2, NULL, NULL);
		std::string narrowString(buffer, size);
		delete[] buffer;
		return narrowString;
	}
	std::wstring convertToUTF16(std::string narrowString) const {
		int oldLength = static_cast<int>(narrowString.length());
		wchar_t* buffer = new wchar_t[oldLength];
		int size = MultiByteToWideChar(CP_UTF8, 0, narrowString.c_str(), oldLength, buffer, oldLength);
		std::wstring wideString(buffer, size);
		delete[] buffer;
		return wideString;
	}
};

//void (*callback)(const TTLibWrapper::ButtonInfo&))

template <typename T>
void TTLibWrapper::forEachGroup(T callback) const {
	TTLibWrapper::ButtonGroupInfo bgi;

	if (!TTLib_ManipulationStart())
		throw Exception{ EXCEPTION_STRING + " TTLib_ManipulationStart" };
	bgi.taskbar = TTLib_GetMainTaskbar();
	if (bgi.taskbar == NULL)
		throw Exception{ EXCEPTION_STRING + " TTLib_GetMainTaskbar" };

	int buttonGroupCount;
	if (!TTLib_GetButtonGroupCount(bgi.taskbar, &buttonGroupCount))
		throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonGroupCount" };

	WCHAR tmpAppId[MAX_APPID_LENGTH];
	for (int i = 0; i < buttonGroupCount; ++i) {
		bgi.index = i;
		bgi.handle = TTLib_GetButtonGroup(bgi.taskbar, i);
		if (bgi.handle == NULL)
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonGroup: " + to_string(bgi.index) };

		SIZE_T length = TTLib_GetButtonGroupAppId(bgi.handle, tmpAppId, MAX_APPID_LENGTH);
		bgi.appId = convertToUTF8(tmpAppId);

		if (!TTLib_GetButtonCount(bgi.handle, &bgi.buttonCount))
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonCount: " + to_string(bgi.index) };

		if (!callback(bgi))
			break;
	}

	TTLib_ManipulationEnd();
}

template <typename T>
bool TTLibWrapper::forEachInGroup(const TTLibWrapper::ButtonGroupInfo& bgi, T callback) const {
	TTLibWrapper::ButtonInfo bi(bgi);

	int buttonCount = bgi.buttonCount;
	for (int i = 0; i < buttonCount; ++i) {
		bi.index = i;
		bi.handle = TTLib_GetButton(bgi.handle, i);
		if (bi.handle == NULL)
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButton: " + to_string(bgi.index) + " " + to_string(bi.index) };

		bi.windowHandle = TTLib_GetButtonWindow(bi.handle);
		if (bi.windowHandle == NULL)
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButton: " + to_string(bgi.index) + " " + to_string(bi.index) };

		if (!callback(bi))
			break;
	}

	return true;
}

template <typename T>
void TTLibWrapper::forEach(T callback) const {
	using namespace std::placeholders;
	forEachGroup(std::bind(&TTLibWrapper::forEachInGroup<T>, this, _1, callback));
}

//#undef TTLIBWRAPPER_H_INSIDE
//#endif //TTLIBWRAPPER_H
