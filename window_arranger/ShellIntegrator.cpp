#include "pch.h"
#include "ShellIntegrator.h"

#include <propsys.h>
#include <propkey.h>
#include <initguid.h>
#include <tlhelp32.h>

#include <thread>


const std::string ShellIntegrator::CLASSNAME = "ShellIntegrator";


ShellIntegrator::ShellIntegrator() try {
	ttlib.loadIntoExplorer();
}
catch (const TTLibWrapper::Exception& e) {
	throw Exception{ EXCEPTION_STRING + " | " + e.str };
}

ShellIntegrator::~ShellIntegrator() {
	try {
		ttlib.unloadFromExplorer();
	}
	catch (const TTLibWrapper::Exception&) {
		// don't throw from destructor
	}
}


void ShellIntegrator::lock() {
	if (!locked) {
		try {
			ttlib.manipulationStart();
			locked = true;
		}
		catch (const TTLibWrapper::Exception& e) {
			throw Exception{ EXCEPTION_STRING + " | " + e.str };
		}
	}
}

void ShellIntegrator::unlock() {
	if (locked) {
		try {
			ttlib.manipulationEnd();
			locked = false;
		}
		catch (const TTLibWrapper::Exception& e) {
			throw Exception{ EXCEPTION_STRING + " | " + e.str };
		}
	}
}


DWORD ShellIntegrator::getParentProcessId() {
	DWORD dwCurrentProcessId = GetCurrentProcessId();;
	DWORD dwParentProcessId = 0;

	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		throw Exception{ EXCEPTION_STRING + ": CreateToolhelp32Snapshot: " + to_string(GetLastError()) };
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hProcessSnap, &pe32)) {
		CloseHandle(hProcessSnap);
		throw Exception{ EXCEPTION_STRING + ": Process32First: " + to_string(GetLastError()) };
	}

	do {
		if (dwCurrentProcessId == pe32.th32ProcessID) {
			dwParentProcessId = pe32.th32ParentProcessID;
			break;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);

	if (!dwParentProcessId)
		throw Exception{ EXCEPTION_STRING + ": can't find parent process" };
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
	std::string error;
	HRESULT hr;

	std::thread th([this, processId, &appId, &hr, &error] {
		hr = CoInitialize(NULL);

		if (SUCCEEDED(hr)) {
			IAppResolver_8* CAppResolver;
			hr = CoCreateInstance(CLSID_StartMenuCacheAndAppResolver, NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER, IID_IAppResolver_8, (void **)&CAppResolver);

			if (SUCCEEDED(hr)) {
				WCHAR* pszAppId;
				hr = CAppResolver->GetAppIDForProcess(processId, &pszAppId, NULL, NULL, NULL);

				if (SUCCEEDED(hr)) {
					try {
						appId = convertToUTF8(pszAppId);
						CoTaskMemFree(pszAppId);
					}
					catch (const UTFConversionException& e) {
						hr = -1;
						error = e.str;
					}
				}

				CAppResolver->Release();
			}

			CoUninitialize();
		}
	});

	th.join();

	if (FAILED(hr))
		throw Exception{ EXCEPTION_STRING + " | " + error };
	return appId;
}

void ShellIntegrator::setWindowAppId(HWND hWnd, std::string_view appId) const {
	std::wstring wAppId;

	try {
		wAppId = convertToUTF16(appId);
	}
	catch (const UTFConversionException& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}

	IPropertyStore *pps;
	PROPVARIANT pv;
	HRESULT hr;

	hr = SHGetPropertyStoreForWindow(hWnd, IID_PPV_ARGS(&pps));
	if (SUCCEEDED(hr)) {
		pv.vt = VT_LPWSTR;
		pv.pwszVal = const_cast<wchar_t*>(wAppId.c_str());

		hr = pps->SetValue(PKEY_AppUserModel_ID, pv);
		if (SUCCEEDED(hr))
			hr = pps->Commit();

		pps->Release();
	}

	if (FAILED(hr))
		throw Exception{ EXCEPTION_STRING };
}

void ShellIntegrator::resetWindowAppId(HWND hWnd) const {
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

	if (FAILED(hr))
		throw Exception{ EXCEPTION_STRING };
}

void ShellIntegrator::moveButtonInGroup(const ButtonGroupInfo& bgi, int indexFrom, int indexTo) {
	if (indexFrom < 0 || indexFrom >= bgi.buttonCount || indexTo < 0 || indexTo >= bgi.buttonCount)
		throw Exception{ EXCEPTION_STRING + ": incorrect indexes: " + to_string(bgi.index) + " " + to_string(indexFrom) + " " + to_string(indexTo) + " " + to_string(bgi.buttonCount) };

	try {
		ttlib.buttonMoveInButtonGroup(bgi.handle, indexFrom, indexTo);
	}
	catch (const TTLibWrapper::Exception& e) {
		throw Exception{ EXCEPTION_STRING + ": " + to_string(bgi.index) + " " + to_string(indexFrom) + " " + to_string(indexTo) + " | " + e.str };
	}
}

void ShellIntegrator::sleep(int msecs) {
	Sleep(msecs);
}
