#pragma once

//#ifndef TTLIBWRAPPER_H
//#define TTLIBWRAPPER_H
//#define TTLIBWRAPPER_H_INSIDE

#include <TTLib.h>

using std::to_string;
#define EXCEPTION_STRING CLASSNAME + "::" + __func__

class TTLibWrapper
{
private:
	static const std::string CLASSNAME;

public:
	struct ButtonInfo {
		HANDLE taskbar;
		HANDLE buttonGroup;
		std::wstring appId;
		int buttonIndex;
		HANDLE button;
		HANDLE buttonWindow;
	};

	TTLibWrapper();
	~TTLibWrapper();
	template <typename T> void forEach(T callback);

	struct Exception { std::string str; };
};

//void (*callback)(const TTLibWrapper::ButtonInfo&))
template <typename T>
void TTLibWrapper::forEach(T callback) {
	TTLibWrapper::ButtonInfo bi{};
	bi.taskbar = TTLib_GetMainTaskbar();
	if (bi.taskbar == NULL)
		throw Exception{ EXCEPTION_STRING + " TTLib_GetMainTaskbar" };

	int buttonGroupCount;
	if (!TTLib_GetButtonGroupCount(bi.taskbar, &buttonGroupCount))
		throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonGroupCount" };

	WCHAR tmpAppId[MAX_APPID_LENGTH];
	for (int i = 0; i < buttonGroupCount; ++i) {
		bi.buttonGroup = TTLib_GetButtonGroup(bi.taskbar, i);
		if (bi.buttonGroup == NULL)
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonGroup: " + to_string(i) };

		SIZE_T length = TTLib_GetButtonGroupAppId(bi.buttonGroup, tmpAppId, MAX_APPID_LENGTH);
		bi.appId = tmpAppId;

		int buttonCount;
		if (!TTLib_GetButtonCount(bi.buttonGroup, &buttonCount))
			throw Exception{ EXCEPTION_STRING + " TTLib_GetButtonCount: " + to_string(i) };

		for (int j = 0; j < buttonCount; ++j) {
			bi.buttonIndex = j;
			bi.button = TTLib_GetButton(bi.buttonGroup, j);
			if (bi.button == NULL)
				throw Exception{ EXCEPTION_STRING + " TTLib_GetButton: " + to_string(i) + " " + to_string(j) };

			bi.buttonWindow = TTLib_GetButtonWindow(bi.button);
			if (bi.buttonWindow == NULL)
				throw Exception{ EXCEPTION_STRING + " TTLib_GetButton: " + to_string(i) + " " + to_string(j) };

			callback(bi);
		}
	}
}


//#undef TTLIBWRAPPER_H_INSIDE
//#endif //TTLIBWRAPPER_H
