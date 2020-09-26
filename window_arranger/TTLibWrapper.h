#pragma once

#define EXCEPTION_STRING CLASSNAME + "::" + __func__


class TTLibWrapper {
public:
	struct Exception { std::string str; };

	//////////////////////////////////////////////////////////////////////////
	// Initialization

	TTLibWrapper();
	~TTLibWrapper();

	//////////////////////////////////////////////////////////////////////////
	// Explorer injection

	void loadIntoExplorer() const;
	bool isLoadedIntoExplorer() const;
	void unloadFromExplorer() const;

	//////////////////////////////////////////////////////////////////////////
	// Manipulation

	enum GROUPTYPE {
		GROUPTYPE_UNKNOWN = 0,
		GROUPTYPE_NORMAL,
		GROUPTYPE_PINNED,
		GROUPTYPE_COMBINED,
		GROUPTYPE_TEMPORARY,
	};

	void manipulationStart() const;
	void manipulationEnd() const;

	HANDLE getMainTaskbar() const;
	int getSecondaryTaskbarCount() const;
	HANDLE getSecondaryTaskbar(int nIndex) const;
	HWND getTaskListWindow(HANDLE hTaskbar) const;
	HWND getTaskbarWindow(HANDLE hTaskbar) const;
	HMONITOR getTaskbarMonitor(HANDLE hTaskbar) const;

	int getButtonGroupCount(HANDLE hTaskbar) const;
	HANDLE getButtonGroup(HANDLE hTaskbar, int nIndex) const;
	HANDLE getActiveButtonGroup(HANDLE hTaskbar) const;
	HANDLE getTrackedButtonGroup(HANDLE hTaskbar) const;
	void buttonGroupMove(HANDLE hTaskbar, int nIndexFrom, int nIndexTo) const;
	HANDLE getButtonGroupTaskbar(HANDLE hButtonGroup) const;
	RECT getButtonGroupRect(HANDLE hButtonGroup) const;
	GROUPTYPE getButtonGroupType(HANDLE hButtonGroup) const;
	std::wstring getButtonGroupAppId(HANDLE hButtonGroup) const;

	int getButtonCount(HANDLE hButtonGroup) const;
	HANDLE getButton(HANDLE hButtonGroup, int nIndex) const;
	HANDLE getActiveButton(HANDLE hTaskbar) const;
	HANDLE getTrackedButton(HANDLE hTaskbar) const;
	void buttonMoveInButtonGroup(HANDLE hButtonGroup, int nIndexFrom, int nIndexTo) const;
	HWND getButtonWindow(HANDLE hButton) const;

	//////////////////////////////////////////////////////////////////////////
	// Lists

	enum LIST {
		LIST_LABEL = 0,
		LIST_GROUP,
		LIST_GROUPPINNED,
		LIST_COMBINE,
	};

	enum LIST_VALUE {
		LIST_LABEL_NEVER = 0,
		LIST_LABEL_ALWAYS,

		LIST_GROUP_NEVER = 0,
		LIST_GROUP_ALWAYS,

		LIST_GROUPPINNED_NEVER = 0,
		LIST_GROUPPINNED_ALWAYS,

		LIST_COMBINE_NEVER = 0,
		LIST_COMBINE_ALWAYS,
	};

	void addAppIdToList(LIST nList, std::wstring_view appId, LIST_VALUE nListValue) const;
	void removeAppIdFromList(LIST nList, std::wstring_view appId) const;
	LIST_VALUE getAppIdListValue(LIST nList, std::wstring_view appId) const;


private:
	static const std::string CLASSNAME;

	HINSTANCE libInstance;
	std::wstring libPath;
	static const int rOK = 0;


#define TTLIBCALL __stdcall

	DWORD (TTLIBCALL *rInit)();
	BOOL (TTLIBCALL *rUninit)();

	DWORD (TTLIBCALL *rLoadIntoExplorer)();
	BOOL (TTLIBCALL *rIsLoadedIntoExplorer)();
	BOOL (TTLIBCALL *rUnloadFromExplorer)();


	BOOL (TTLIBCALL *rManipulationStart)();
	BOOL (TTLIBCALL *rManipulationEnd)();

	HANDLE (TTLIBCALL *rGetMainTaskbar)();
	BOOL (TTLIBCALL *rGetSecondaryTaskbarCount)(int *pnCount);
	HANDLE (TTLIBCALL *rGetSecondaryTaskbar)(int nIndex);
	HWND (TTLIBCALL *rGetTaskListWindow)(HANDLE hTaskbar);
	HWND (TTLIBCALL *rGetTaskbarWindow)(HANDLE hTaskbar);
	HMONITOR (TTLIBCALL *rGetTaskbarMonitor)(HANDLE hTaskbar);

	BOOL (TTLIBCALL *rGetButtonGroupCount)(HANDLE hTaskbar, int *pnCount);
	HANDLE (TTLIBCALL *rGetButtonGroup)(HANDLE hTaskbar, int nIndex);
	HANDLE (TTLIBCALL *rGetActiveButtonGroup)(HANDLE hTaskbar);
	HANDLE (TTLIBCALL *rGetTrackedButtonGroup)(HANDLE hTaskbar);
	BOOL (TTLIBCALL *rButtonGroupMove)(HANDLE hTaskbar, int nIndexFrom, int nIndexTo);
	BOOL (TTLIBCALL *rGetButtonGroupTaskbar)(HANDLE hButtonGroup, HANDLE *phTaskbar);
	BOOL (TTLIBCALL *rGetButtonGroupRect)(HANDLE hButtonGroup, RECT *pRect);
	BOOL (TTLIBCALL *rGetButtonGroupType)(HANDLE hButtonGroup, GROUPTYPE *pnType);
	SIZE_T (TTLIBCALL *rGetButtonGroupAppId)(HANDLE hButtonGroup, WCHAR *pszAppId, SIZE_T nMaxSize);

	BOOL (TTLIBCALL *rGetButtonCount)(HANDLE hButtonGroup, int *pnCount);
	HANDLE (TTLIBCALL *rGetButton)(HANDLE hButtonGroup, int nIndex);
	HANDLE (TTLIBCALL *rGetActiveButton)(HANDLE hTaskbar);
	HANDLE (TTLIBCALL *rGetTrackedButton)(HANDLE hTaskbar);
	BOOL (TTLIBCALL *rButtonMoveInButtonGroup)(HANDLE hButtonGroup, int nIndexFrom, int nIndexTo);
	HWND (TTLIBCALL *rGetButtonWindow)(HANDLE hButton);


	BOOL (TTLIBCALL *rAddAppIdToList)(LIST nList, const WCHAR *pszAppId, LIST_VALUE nListValue);
	BOOL (TTLIBCALL *rRemoveAppIdFromList)(LIST nList, const WCHAR *pszAppId);
	BOOL (TTLIBCALL *rGetAppIdListValue)(LIST nList, const WCHAR *pszAppId, LIST_VALUE *pnListValue);


	template<typename T>
	void loadFunction(T& var, const std::string& varName) {
		var = (T)GetProcAddress(libInstance, ("TTLib_" + varName).c_str());
		if (var == NULL)
			throw Exception{ EXCEPTION_STRING + " can't find: " + varName };
	}
};
