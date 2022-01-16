#pragma once
#include "ITuniacServicePlugin.h"

typedef struct
{
	HMODULE					hDLL;
	//HANDLE					hThread;
	//DWORD					dwThreadId;
	ITuniacServicePlugin*	pPlugin;
	TCHAR					szName[64];
	TCHAR					szDllFile[64];
	unsigned long			ulFlags;
	bool					bEnabled;
} ServicePluginEntry;

class CServicePluginManager :
	public ITuniacServicePluginHelper
{
protected:
	Array<ServicePluginEntry, 3>	m_PluginArray;
	TCHAR							m_PluginPath[MAX_PATH];

public:
	CServicePluginManager(void);
	~CServicePluginManager(void);

	bool			Initialize(void);
	bool			Shutdown(void);

	unsigned int	GetNumPlugins(void);
	ServicePluginEntry* GetPluginAtIndex(unsigned int iPlugin);

	bool			IsPluginEnabled(unsigned int iPlugin);
	bool			EnablePlugin(unsigned int iPlugin, bool bEnabled);

	//void			PostMessage(UINT Msg, WPARAM wParam, LPARAM lParam);

	//void* GetVariable(Variable eVar);
	//void			GetTrackInfo(LPTSTR szDest, unsigned int iDestSize, LPTSTR szFormat, unsigned int iIndex);
	//bool			Navigate(int iFromCurrent);

	void			LogMessage(LPTSTR szModuleName, LPTSTR szMessage);

	//HINSTANCE		GetMainInstance(void);
	//HWND			GetMainWindow(void);

	bool			PreferencesGet(LPCTSTR szSubKey, LPCTSTR lpValueName, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
	bool			PreferencesSet(LPCTSTR szSubKey, LPCTSTR lpValueName, DWORD dwType, const BYTE* lpData, DWORD cbData);

};
