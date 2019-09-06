#include "pch.h"
#include "TTLibWrapper.h"

#include <propsys.h>
#include <propkey.h>

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


bool TTLibWrapper::setWindowAppId(HWND hWnd, const std::string& appId) const {
	if (!appId.empty()) {
		IPropertyStore *pps;
		PROPVARIANT pv;
		HRESULT hr;

		hr = SHGetPropertyStoreForWindow(hWnd, IID_PPV_ARGS(&pps));
		if (SUCCEEDED(hr)) {
			pv.vt = VT_LPWSTR;
			std::wstring wAppId = convertToUTF16(appId);
			pv.pwszVal = const_cast<wchar_t*>(wAppId.c_str());

			hr = pps->SetValue(PKEY_AppUserModel_ID, pv);
			if (SUCCEEDED(hr))
				hr = pps->Commit();

			pps->Release();
		}

		return SUCCEEDED(hr);
	}
	else
		return false;
}

bool TTLibWrapper::moveButtonInGroup(const ButtonGroupInfo& bgi, int indexFrom, int indexTo) {
	if (indexTo >= 0 && indexTo < bgi.buttonCount && indexFrom >= 0 && indexFrom < bgi.buttonCount)
		return TTLib_ButtonMoveInButtonGroup(bgi.handle, indexFrom, indexTo);
	else
		return false;
}