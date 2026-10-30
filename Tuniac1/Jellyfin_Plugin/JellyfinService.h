#pragma once

#include "ITuniacServicePlugin.h"

struct HttpResponse;

#define JELLYFIN_CLIENT_NAME		TEXT("Tuniac")
#define JELLYFIN_CLIENT_VERSION		TEXT("1.0")

// Settings live under services\Jellyfin; each server's login under services\Jellyfin\Servers\<ServerId>
#define JELLYFIN_PREFS_KEY			TEXT("Jellyfin")

enum JellyfinResult
{
	JELLYFIN_OK = 0,
	JELLYFIN_ERROR_INVALIDARGS,		// bad input, e.g. malformed server URL
	JELLYFIN_ERROR_CONNECT,			// server unreachable, timeout, DNS
	JELLYFIN_ERROR_CERTIFICATE,		// HTTPS certificate rejected
	JELLYFIN_ERROR_UNAUTHORIZED,	// wrong credentials or token no longer valid
	JELLYFIN_ERROR_RESPONSE			// server answered with something unexpected
};

class CJellyfinService :
	public ITuniacServicePlugin
{

protected:
	/*HANDLE					m_hThread;
	HWND					m_hWnd;

	ATOM					m_aHotkeyShow;

	APPBARDATA				m_abd;

	unsigned long			m_ShowTimeMS;
	unsigned long			m_FadeTimeMS;
	BYTE					m_Alpha;

	RECT					m_rcHit;

	HFONT					m_SmallFont;
	HFONT					m_SmallFontB;
	HFONT					m_SmallFontU;*/

	ITuniacServicePluginHelper* m_pHelper;

	TCHAR					m_szDeviceId[64];
	TCHAR					m_szDeviceName[128];
	bool					m_bAllowInvalidCertificates;

	// The active server and its session. Logged in when m_szToken is non-empty.
	TCHAR					m_szServerId[64];
	TCHAR					m_szServerUrl[256];
	TCHAR					m_szUserId[64];
	TCHAR					m_szUserName[128];
	TCHAR					m_szToken[128];

	TCHAR					m_szLastError[256];

	void					LoadSettings(void);
	void					LoadServer(LPCTSTR szServerId);
	void					SaveServer(void);

	bool					GetPrefString(LPCTSTR szSubKey, LPCTSTR szName, LPTSTR szDest, size_t cchDest);
	bool					SetPrefString(LPCTSTR szSubKey, LPCTSTR szName, LPCTSTR szValue);

	bool					BuildAuthHeader(LPTSTR szDest, size_t cchDest, LPCTSTR szToken);
	bool					BuildUrl(LPTSTR szDest, size_t cchDest, LPCTSTR szServerUrl, LPCTSTR szPath);

	JellyfinResult			ResultFromResponse(HttpResponse& response, LPCTSTR szAction);
	void					SetError(LPCTSTR szFormat, ...);

	/*BOOL					m_bAllowInhibit;
	BOOL					m_bManualTrigger;
	BOOL					m_bManualBlindTrigger;
	BOOL					m_bAutoTrigger;
	BOOL					m_bAutoBlindTrigger;

	TCHAR					m_szNormalFormatString[128];
	TCHAR					m_szStreamFormatString[128];

	bool					m_bInhibit;*/

	//static LRESULT CALLBACK	WndProcStub(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	//LRESULT CALLBACK		WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK	DlgProcStub(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK		DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

	//static unsigned long __stdcall	ThreadStub(void* in);
	//unsigned long			ThreadProc(void);

	//void					RePaint(HWND hWnd);
	//void					ResetWindow();

public:
	CJellyfinService();
	~CJellyfinService();

	void				Destroy(void);

	LPTSTR				GetPluginName(void);
	unsigned long		GetFlags(void);

	bool				SetHelper(ITuniacServicePluginHelper* pHelper);

	//HANDLE				CreateThread(LPDWORD lpThreadId);
	//HWND				GetPluginWindow(void);

	bool				About(HWND hWndParent);
	bool				Configure(HWND hWndParent);

	// Authenticate against a Jellyfin server (e.g. "https://jellyfin.example.com") and make it the
	// active server. The token, user and server URL are persisted per ServerId; the password is not.
	JellyfinResult		Login(LPCTSTR szServerUrl, LPCTSTR szUsername, LPCTSTR szPassword);

	// Check the stored token against the active server. A rejected token logs us out locally;
	// a network failure leaves the token alone.
	JellyfinResult		ValidateSession(void);

	// Revoke the token on the server (best effort) and forget it locally.
	JellyfinResult		Logout(void);

	bool				IsLoggedIn(void)			{ return m_szToken[0] != TEXT('\0'); }
	LPCTSTR				GetServerUrl(void)			{ return m_szServerUrl; }
	LPCTSTR				GetLoggedInUser(void)		{ return m_szUserName; }
	LPCTSTR				GetLastErrorText(void)		{ return m_szLastError; }

	bool				GetAllowInvalidCertificates(void)	{ return m_bAllowInvalidCertificates; }
	void				SetAllowInvalidCertificates(bool bAllow);
};
