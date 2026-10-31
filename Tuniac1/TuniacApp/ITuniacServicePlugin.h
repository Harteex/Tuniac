#pragma once

#define ITUNIACSERVICEPLUGIN_VERSION	MAKELONG(0,4)

class ITuniacServicePluginHelper
{
public:

	virtual void		LogMessage(LPTSTR szModuleName, LPTSTR szMessage) = 0;
	//virtual HINSTANCE	GetMainInstance(void) = 0;
	//virtual HWND		GetMainWindow(void) = 0;

	virtual bool		PreferencesGet(LPCTSTR szSubKey, LPCTSTR lpValueName, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) = 0;
	virtual bool		PreferencesSet(LPCTSTR szSubKey, LPCTSTR lpValueName, DWORD dwType, const BYTE* lpData, DWORD cbData) = 0;
};


#define SERVICEPLUGINFLAGS_ABOUT				0x00000001
#define SERVICEPLUGINFLAGS_CONFIG				0x00000002

// Size in characters of the buffer TranslateSource fills
#define SERVICE_SOURCE_MAX						2048


class ITuniacServicePlugin
{
public:
	virtual void			Destroy(void) = 0;

	virtual LPTSTR			GetPluginName(void) = 0;
	virtual unsigned long	GetFlags(void) = 0;

	virtual bool			SetHelper(ITuniacServicePluginHelper* pHelper) = 0;

	//virtual HANDLE			CreateThread(LPDWORD lpThreadId) = 0;
	//virtual	HWND			GetPluginWindow(void) = 0;

	virtual bool			About(HWND hWndParent) = 0;
	virtual bool			Configure(HWND hWndParent) = 0;

	// Library entries can hold service links (e.g. jellyfin://<server>/<item>). Just before playback
	// the host asks each service whether it owns the link and, if so, to resolve it into something
	// a decoder can open. Return false from TranslateSource if it can't be played right now
	// (e.g. not logged in); the host then treats it as a failed track.
	virtual bool			CanHandle(LPCTSTR szSource) = 0;
	virtual bool			TranslateSource(LPTSTR szDest, size_t cchDest, LPCTSTR szSource) = 0;

	// No decoder could open what TranslateSource returned. Find out why if possible and log it;
	// decoders usually can't say more than "failed".
	virtual void			SourceFailed(LPCTSTR szSource) = 0;
};

typedef ITuniacServicePlugin* (*CreateTuniacServicePluginFunc)(void);
extern "C" __declspec(dllexport) ITuniacServicePlugin * CreateTuniacServicePlugin(void);

typedef unsigned long (*GetTuniacServicePluginVersionFunc)(void);
extern "C" __declspec(dllexport) unsigned long		GetTuniacServicePluginVersion(void);
