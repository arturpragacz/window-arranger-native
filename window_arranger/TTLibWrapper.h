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
		HANDLE buttonGroup;
		std::string appId;
		int buttonCount;
	};
	struct ButtonInfo {
		ButtonGroupInfo group;
		int buttonIndex;
		HANDLE button;
		HWND buttonWindow;
	};

	TTLibWrapper();
	~TTLibWrapper();

	static std::function<bool(const ButtonGroupInfo&)> Noop;
	template <typename T> void forEach(T callback) const;
	template <typename T, typename T1, typename T2>
		void forEach(T1 groupPreCallback, T callback, T2 groupPostCallback) const;

	bool setWindowAppId(HWND hWnd, const std::string& appId) const;

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

template <typename T> void TTLibWrapper::forEach(T callback) const {
	forEach<T&>(Noop, callback, Noop);
}

//void (*callback)(const TTLibWrapper::ButtonInfo&))
template <typename T, typename T1, typename T2>
void TTLibWrapper::forEach(T1 groupPreCallback, T callback, T2 groupPostCallback) const {
	TTLibWrapper::ButtonInfo bi{};
	TTLibWrapper::ButtonGroupInfo& bgi = bi.group;

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
		bgi.buttonGroup = TTLib_GetButtonGroup(bgi.taskbar, i);
		if (bgi.buttonGroup == NULL)
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonGroup: " + to_string(i) };

		SIZE_T length = TTLib_GetButtonGroupAppId(bgi.buttonGroup, tmpAppId, MAX_APPID_LENGTH);
		bgi.appId = convertToUTF8(tmpAppId);

		int buttonCount;
		if (!TTLib_GetButtonCount(bgi.buttonGroup, &buttonCount))
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonCount: " + to_string(i) };

		if (groupPreCallback(bgi))
			for (int j = 0; j < buttonCount; ++j) {
				bi.buttonIndex = j;
				bi.button = TTLib_GetButton(bgi.buttonGroup, j);
				if (bi.button == NULL)
					throw Exception{ EXCEPTION_STRING + " TTLib_GetButton: " + to_string(i) + " " + to_string(j) };

				bi.buttonWindow = TTLib_GetButtonWindow(bi.button);
				if (bi.buttonWindow == NULL)
					throw Exception{ EXCEPTION_STRING + " TTLib_GetButton: " + to_string(i) + " " + to_string(j) };

				if (!callback(bi))
					break;
			}

		if (!groupPostCallback(bgi))
			break;
	}

	TTLib_ManipulationEnd();
}


//#undef TTLIBWRAPPER_H_INSIDE
//#endif //TTLIBWRAPPER_H
