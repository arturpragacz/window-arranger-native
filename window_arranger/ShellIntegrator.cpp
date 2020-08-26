#include "pch.h"
#include "ShellIntegrator.h"

#include <propsys.h>
#include <propkey.h>
#include <initguid.h>
#include <tlhelp32.h>

#include <thread>


const std::string ShellIntegrator::CLASSNAME = "ShellIntegrator";


ShellIntegrator::ShellIntegrator() {
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

ShellIntegrator::~ShellIntegrator() {
	TTLib_UnloadFromExplorer();
	TTLib_Uninit();
}


void ShellIntegrator::lock() {
	if (!locked) {
		if (!TTLib_ManipulationStart())
			throw Exception{ EXCEPTION_STRING + " TTLib_ManipulationStart" };
		locked = true;
	}
}

void ShellIntegrator::unlock() {
	if (locked) {
		TTLib_ManipulationEnd();
		locked = false;
	}
}


DWORD ShellIntegrator::getParentProcessId() {
	DWORD dwCurrentProcessId = GetCurrentProcessId();;
	DWORD dwParentProcessId = 0;

	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hProcessSnap, &pe32)) {
		CloseHandle(hProcessSnap);
		return 0;
	}

	do {
		if (dwCurrentProcessId == pe32.th32ProcessID) {
			dwParentProcessId = pe32.th32ParentProcessID;
			break;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	return dwParentProcessId;
}

// {c8900b66-a973-584b-8cae-355b7f55341b}
DEFINE_GUID(CLSID_StartMenuCacheAndAppResolver, 0x660b90c8, 0x73a9, 0x4b58, 0x8c, 0xae, 0x35, 0x5b, 0x7f, 0x55, 0x34, 0x1b);
// {de25675a-72de-44b4-9373-05170450c140}
DEFINE_GUID(IID_IAppResolver_8, 0xde25675a, 0x72de, 0x44b4, 0x93, 0x73, 0x05, 0x17, 0x04, 0x50, 0xc1, 0x40);
struct IAppResolver_8 : IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetAppIDForShortcut() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAppIDForShortcutObject() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAppIDForWindow(HWND hWnd, WCHAR **pszAppId, void *pUnknown1, void *pUnknown2, void *pUnknown3) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAppIDForProcess(DWORD dwProcessId, WCHAR **pszAppId, void *pUnknown1, void *pUnknown2, void *pUnknown3) = 0;
};

std::string ShellIntegrator::getProcessAppId(DWORD processId) {
	std::string appId;

	std::thread th([this, processId, &appId] {
		HRESULT hr = CoInitialize(NULL);

		if (SUCCEEDED(hr)) {
			IAppResolver_8* CAppResolver;
			hr = CoCreateInstance(CLSID_StartMenuCacheAndAppResolver, NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER, IID_IAppResolver_8, (void **)&CAppResolver);

			if (SUCCEEDED(hr)) {
				WCHAR* pszAppId;
				hr = CAppResolver->GetAppIDForProcess(processId, &pszAppId, NULL, NULL, NULL);

				if (SUCCEEDED(hr)) {
					appId = convertToUTF8(pszAppId);
					CoTaskMemFree(pszAppId);
				}

				CAppResolver->Release();
			}

			CoUninitialize();
		}
	});

	th.join();
	return appId;
}

bool ShellIntegrator::setWindowAppId(HWND hWnd, std::string_view appId) const {
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

bool ShellIntegrator::resetWindowAppId(HWND hWnd) const {
	IPropertyStore *pps;
	PROPVARIANT pv;
	HRESULT hr;

	hr = SHGetPropertyStoreForWindow(hWnd, IID_PPV_ARGS(&pps));
	if (SUCCEEDED(hr)) {
		PropVariantInit(&pv);

		hr = pps->SetValue(PKEY_AppUserModel_ID, pv);
		if (SUCCEEDED(hr))
			hr = pps->Commit();

		pps->Release();
	}

	return SUCCEEDED(hr);
}

bool ShellIntegrator::moveButtonInGroup(const ButtonGroupInfo& bgi, int indexFrom, int indexTo) {
	if (indexTo >= 0 && indexTo < bgi.buttonCount && indexFrom >= 0 && indexFrom < bgi.buttonCount)
		return TTLib_ButtonMoveInButtonGroup(bgi.handle, indexFrom, indexTo);
	else
		return false;
}

void ShellIntegrator::sleep(int msecs) {
	Sleep(msecs);
}
