#pragma once

#define ITUNIACSERVICEPLUGIN_VERSION	MAKELONG(0,4)

// A track a service adds to the media library. The host stores it as-is (no tag reading), so
// fill in whatever the service knows; leave the rest zeroed.
typedef struct
{
	TCHAR				szURL[MAX_PATH];		// service link, e.g. jellyfin://<server>/<item>
	TCHAR				szTitle[128];
	TCHAR				szArtist[128];
	TCHAR				szAlbum[128];
	TCHAR				szAlbumArtist[128];
	TCHAR				szGenre[128];
	TCHAR				szComposer[128];
	TCHAR				szFileType[16];			// e.g. "flac"
	unsigned long		ulYear;
	unsigned short		usTrack[2];				// track, track count (0 if unknown)
	unsigned short		usDisc[2];				// disc, disc count (0 if unknown)
	unsigned long		ulPlaybackTimeMS;		// 0 if unknown
	unsigned long		ulBitRate;				// bits per second, 0 if unknown
	unsigned long		ulSampleRate;
	unsigned long		ulChannels;
	unsigned long		ulBitsPerSample;
} ServiceTrack;

class ITuniacServicePluginHelper
{
public:

	virtual void		LogMessage(LPTSTR szModuleName, LPTSTR szMessage) = 0;
	//virtual HINSTANCE	GetMainInstance(void) = 0;
	//virtual HWND		GetMainWindow(void) = 0;

	virtual bool		PreferencesGet(LPCTSTR szSubKey, LPCTSTR lpValueName, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) = 0;
	virtual bool		PreferencesSet(LPCTSTR szSubKey, LPCTSTR lpValueName, DWORD dwType, const BYTE* lpData, DWORD cbData) = 0;

	// Add tracks to the media library. Links already in the library are skipped. Returns how
	// many were added.
	virtual unsigned long	AddToLibrary(ServiceTrack* pTracks, unsigned long ulCount) = 0;
};


#define SERVICEPLUGINFLAGS_ABOUT				0x00000001
#define SERVICEPLUGINFLAGS_CONFIG				0x00000002
#define SERVICEPLUGINFLAGS_IMPORT				0x00000004	// has Import(); the host lists it under File

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

	// File -> "Import from <name>...": let the user pick content and add it with
	// ITuniacServicePluginHelper::AddToLibrary.
	virtual bool			Import(HWND hWndParent) = 0;
};

typedef ITuniacServicePlugin* (*CreateTuniacServicePluginFunc)(void);
extern "C" __declspec(dllexport) ITuniacServicePlugin * CreateTuniacServicePlugin(void);

typedef unsigned long (*GetTuniacServicePluginVersionFunc)(void);
extern "C" __declspec(dllexport) unsigned long		GetTuniacServicePluginVersion(void);
