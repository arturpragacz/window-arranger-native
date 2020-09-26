#include "pch.h"
#include "TTLibWrapper.h"
#include "windows_utils.h"

#include <magic_enum.hpp>

#ifdef _WIN64
static constexpr const wchar_t* TTLibPath = L"TTLib64.dll";
#else
static constexpr const wchar_t* TTLibPath = L"TTLib32.dll";
#endif


const std::string TTLibWrapper::CLASSNAME = "TTLibWrapper";


TTLibWrapper::TTLibWrapper() {
	try {
		libPath = createTmpCopy(TTLibPath, L"TTLib");
	}
	catch (const FileSystemException& e) {
		throw Exception{ EXCEPTION_STRING + " | " + e.str };
	}


	libInstance = LoadLibrary(libPath.c_str());
	if (libInstance == NULL)
		throw Exception{ EXCEPTION_STRING + ": can't load library: " + convertToUTF8(libPath, false) };


	loadFunction(rInit, "Init");
	loadFunction(rUninit, "Uninit");

	loadFunction(rLoadIntoExplorer, "LoadIntoExplorer");
	loadFunction(rIsLoadedIntoExplorer, "IsLoadedIntoExplorer");
	loadFunction(rUnloadFromExplorer, "UnloadFromExplorer");


	loadFunction(rManipulationStart, "ManipulationStart");
	loadFunction(rManipulationEnd, "ManipulationEnd");

	loadFunction(rGetMainTaskbar, "GetMainTaskbar");
	loadFunction(rGetSecondaryTaskbarCount, "GetSecondaryTaskbarCount");
	loadFunction(rGetSecondaryTaskbar, "GetSecondaryTaskbar");
	loadFunction(rGetTaskListWindow, "GetTaskListWindow");
	loadFunction(rGetTaskbarWindow, "GetTaskbarWindow");
	loadFunction(rGetTaskbarMonitor, "GetTaskbarMonitor");

	loadFunction(rGetButtonGroupCount, "GetButtonGroupCount");
	loadFunction(rGetButtonGroup, "GetButtonGroup");
	loadFunction(rGetActiveButtonGroup, "GetActiveButtonGroup");
	loadFunction(rGetTrackedButtonGroup, "GetTrackedButtonGroup");
	loadFunction(rButtonGroupMove, "ButtonGroupMove");
	loadFunction(rGetButtonGroupTaskbar, "GetButtonGroupTaskbar");
	loadFunction(rGetButtonGroupRect, "GetButtonGroupRect");
	loadFunction(rGetButtonGroupType, "GetButtonGroupType");
	loadFunction(rGetButtonGroupAppId, "GetButtonGroupAppId");

	loadFunction(rGetButtonCount, "GetButtonCount");
	loadFunction(rGetButton, "GetButton");
	loadFunction(rGetActiveButton, "GetActiveButton");
	loadFunction(rGetTrackedButton, "GetTrackedButton");
	loadFunction(rButtonMoveInButtonGroup, "ButtonMoveInButtonGroup");
	loadFunction(rGetButtonWindow, "GetButtonWindow");


	loadFunction(rAddAppIdToList, "AddAppIdToList");
	loadFunction(rRemoveAppIdFromList, "RemoveAppIdFromList");
	loadFunction(rGetAppIdListValue, "GetAppIdListValue");


	// Errors returned by TTLib_Init
	enum INIT_ERROR {
		ERR_INIT_ALREADY_INITIALIZED = 1,
		ERR_INIT_REGISTER_MESSAGE,
		ERR_INIT_FILE_MAPPING,
		ERR_INIT_VIEW_MAPPING,
	};

	INIT_ERROR initError = (INIT_ERROR)(rInit());
	if (initError != rOK)
		throw Exception{ EXCEPTION_STRING + ": Init: " + magic_enum::enum_name(initError).data() };
}

TTLibWrapper::~TTLibWrapper() {
	rUninit();
	FreeLibrary(libInstance);
	try {
		deleteFile(libPath.c_str());
	}
	catch (const FileSystemException&) {
		// don't throw from destructor
	}
}


void TTLibWrapper::loadIntoExplorer() const {
	// Errors returned by TTLib_LoadIntoExplorer
	enum LOAD_ERROR {
		ERR_EXE_NOT_INITIALIZED = 1,
		ERR_EXE_ALREADY_LOADED,
		ERR_EXE_FIND_TASKBAR,
		ERR_EXE_OPEN_PROCESS,
		ERR_EXE_VIRTUAL_ALLOC,
		ERR_EXE_WRITE_PROC_MEM,
		ERR_EXE_CREATE_REMOTE_THREAD,
		ERR_EXE_READ_PROC_MEM,

		ERR_INJ_BEFORE_RUN = 101,
		ERR_INJ_BEFORE_GETMODULEHANDLE,
		ERR_INJ_BEFORE_LOADLIBRARY,
		ERR_INJ_BEFORE_GETPROCADDR,
		ERR_INJ_BEFORE_LIBINIT,
		ERR_INJ_GETMODULEHANDLE,
		ERR_INJ_LOADLIBRARY,
		ERR_INJ_GETPROCADDR,

		ERR_LIB_INIT_ALREADY_CALLED = 201,
		ERR_LIB_LIB_VER_MISMATCH,
		ERR_LIB_WIN_VER_MISMATCH,
		ERR_LIB_VIEW_MAPPING,
		ERR_LIB_FIND_IMPORT,
		ERR_LIB_WND_TASKBAR,
		ERR_LIB_WND_TASKSW,
		ERR_LIB_WND_TASKLIST,
		ERR_LIB_WND_THUMB,
		ERR_LIB_MSG_DLL_INIT,
		ERR_LIB_WAITTHREAD,

		ERR_LIB_EXTHREAD_MINHOOK = 301,
		ERR_LIB_EXTHREAD_MINHOOK_PRELOADED,
		ERR_LIB_EXTHREAD_COMFUNCHOOK,
		ERR_LIB_EXTHREAD_REFRESHTASKBAR,
		ERR_LIB_EXTHREAD_MINHOOK_APPLY,
	};

	LOAD_ERROR loadError = (LOAD_ERROR)rLoadIntoExplorer();
	if (loadError != rOK)
		throw Exception{ EXCEPTION_STRING + ": " + magic_enum::enum_name(loadError).data() };
}

bool TTLibWrapper::isLoadedIntoExplorer() const {
	return rIsLoadedIntoExplorer();
}

void TTLibWrapper::unloadFromExplorer() const {
	if (!rUnloadFromExplorer())
		throw Exception{ EXCEPTION_STRING };
}


void TTLibWrapper::manipulationStart() const {
	if (!rManipulationStart())
		throw Exception{ EXCEPTION_STRING };
}

void TTLibWrapper::manipulationEnd() const {
	if (!rManipulationEnd())
		throw Exception{ EXCEPTION_STRING };
}


HANDLE TTLibWrapper::getMainTaskbar() const {
	HANDLE handle = rGetMainTaskbar();
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

int TTLibWrapper::getSecondaryTaskbarCount() const {
	int count;
	if (!rGetSecondaryTaskbarCount(&count))
		throw Exception{ EXCEPTION_STRING };
	return count;
}

HANDLE TTLibWrapper::getSecondaryTaskbar(int nIndex) const {
	HANDLE handle = rGetSecondaryTaskbar(nIndex);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

HWND TTLibWrapper::getTaskListWindow(HANDLE hTaskbar) const {
	HWND handle = rGetTaskListWindow(hTaskbar);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

HWND TTLibWrapper::getTaskbarWindow(HANDLE hTaskbar) const {
	HWND handle = rGetTaskbarWindow(hTaskbar);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

HMONITOR TTLibWrapper::getTaskbarMonitor(HANDLE hTaskbar) const {
	HMONITOR monitor = rGetTaskbarMonitor(hTaskbar);
	if (monitor == NULL)
		throw Exception{ EXCEPTION_STRING };
	return monitor;
}


int TTLibWrapper::getButtonGroupCount(HANDLE hTaskbar) const {
	int count;
	if (!rGetButtonGroupCount(hTaskbar, &count))
		throw Exception{ EXCEPTION_STRING };
	return count;
}

HANDLE TTLibWrapper::getButtonGroup(HANDLE hTaskbar, int nIndex) const {
	HANDLE handle = rGetButtonGroup(hTaskbar, nIndex);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

HANDLE TTLibWrapper::getActiveButtonGroup(HANDLE hTaskbar) const {
	HANDLE handle = rGetActiveButtonGroup(hTaskbar);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

HANDLE TTLibWrapper::getTrackedButtonGroup(HANDLE hTaskbar) const {
	HANDLE handle = rGetTrackedButtonGroup(hTaskbar);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

void TTLibWrapper::buttonGroupMove(HANDLE hTaskbar, int nIndexFrom, int nIndexTo) const {
	if (!rButtonGroupMove(hTaskbar, nIndexFrom, nIndexTo))
		throw Exception{ EXCEPTION_STRING };
}

HANDLE TTLibWrapper::getButtonGroupTaskbar(HANDLE hButtonGroup) const {
	HANDLE handle;
	if (!rGetButtonGroupTaskbar(hButtonGroup, &handle))
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

RECT TTLibWrapper::getButtonGroupRect(HANDLE hButtonGroup) const {
	RECT rect;
	if (!rGetButtonGroupRect(hButtonGroup, &rect))
		throw Exception{ EXCEPTION_STRING };
	return rect;
}

TTLibWrapper::GROUPTYPE TTLibWrapper::getButtonGroupType(HANDLE hButtonGroup) const {
	GROUPTYPE groupType;
	if (!rGetButtonGroupType(hButtonGroup, &groupType))
		throw Exception{ EXCEPTION_STRING };
	return groupType;
}

std::wstring TTLibWrapper::getButtonGroupAppId(HANDLE hButtonGroup) const {
	const int MAX_APPID_LENGTH = MAX_PATH;
	wchar_t arr[MAX_APPID_LENGTH];

	size_t ssize = rGetButtonGroupAppId(hButtonGroup, arr, MAX_APPID_LENGTH);
	if (!ssize)
		throw Exception{ EXCEPTION_STRING };
	ssize -= 1; // don't count ending '\0'

	return std::wstring{arr, ssize};
}


int TTLibWrapper::getButtonCount(HANDLE hButtonGroup) const {
	int count;
	if (!rGetButtonCount(hButtonGroup, &count))
		throw Exception{ EXCEPTION_STRING };
	return count;
}

HANDLE TTLibWrapper::getButton(HANDLE hButtonGroup, int nIndex) const {
	HANDLE handle = rGetButton(hButtonGroup, nIndex);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

HANDLE TTLibWrapper::getActiveButton(HANDLE hTaskbar) const {
	HANDLE handle = rGetActiveButton(hTaskbar);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

HANDLE TTLibWrapper::getTrackedButton(HANDLE hTaskbar) const {
	HANDLE handle = rGetTrackedButton(hTaskbar);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}

void TTLibWrapper::buttonMoveInButtonGroup(HANDLE hButtonGroup, int nIndexFrom, int nIndexTo) const {
	if (!rButtonMoveInButtonGroup(hButtonGroup, nIndexFrom, nIndexTo))
		throw Exception{ EXCEPTION_STRING };
}

HWND TTLibWrapper::getButtonWindow(HANDLE hButton) const {
	HWND handle = rGetButtonWindow(hButton);
	if (handle == NULL)
		throw Exception{ EXCEPTION_STRING };
	return handle;
}


void TTLibWrapper::addAppIdToList(LIST nList, std::wstring_view appId, LIST_VALUE nListValue) const {
	if (!rAddAppIdToList(nList, appId.data(), nListValue))
		throw Exception{ EXCEPTION_STRING };
}

void TTLibWrapper::removeAppIdFromList(LIST nList, std::wstring_view appId) const {
	if (!rRemoveAppIdFromList(nList, appId.data()))
		throw Exception{ EXCEPTION_STRING };
}

TTLibWrapper::LIST_VALUE TTLibWrapper::getAppIdListValue(LIST nList, std::wstring_view appId) const {
	LIST_VALUE listValue;
	if (!rGetAppIdListValue(nList, appId.data(), &listValue))
		throw Exception{ EXCEPTION_STRING };
	return listValue;
}
