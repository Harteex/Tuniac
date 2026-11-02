#pragma once

#include "ITuniacServicePlugin.h"
#include "rapidjson/fwd.h"

struct HttpResponse;
struct JellyfinItems;
struct TrackList;

#define JELLYFIN_CLIENT_NAME		TEXT("Tuniac")
#define JELLYFIN_CLIENT_VERSION		TEXT("1.0")

// Settings live under services\Jellyfin; each server's login under services\Jellyfin\Servers\<ServerId>
#define JELLYFIN_PREFS_KEY			TEXT("Jellyfin")

// Library links look like jellyfin://<ServerId>/<ItemId>
#define JELLYFIN_SCHEME				TEXT("jellyfin://")
#define JELLYFIN_SCHEME_LEN			11

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

	TCHAR					m_szLastError[512];

	void					LoadSettings(void);
	void					LoadServer(LPCTSTR szServerId);
	void					SaveServer(void);
	bool					GetServerLogin(LPCTSTR szServerId, LPTSTR szUrl, size_t cchUrl, LPTSTR szToken, size_t cchToken);
	void					ForgetLogin(LPCTSTR szServerId);

	bool					GetPrefString(LPCTSTR szSubKey, LPCTSTR szName, LPTSTR szDest, size_t cchDest);
	bool					SetPrefString(LPCTSTR szSubKey, LPCTSTR szName, LPCTSTR szValue);

	bool					BuildAuthHeader(LPTSTR szDest, size_t cchDest, LPCTSTR szToken);
	bool					BuildUrl(LPTSTR szDest, size_t cchDest, LPCTSTR szServerUrl, LPCTSTR szPath);

	JellyfinResult			ResultFromResponse(HttpResponse& response, LPCTSTR szAction);
	void					SetError(LPCTSTR szFormat, ...);

	static LRESULT CALLBACK	DlgProcStub(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK		DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void					UpdateDialog(HWND hDlg, LPCTSTR szNote);
	void					SetDialogBusy(HWND hDlg, LPCTSTR szStatus);

	// import dialog
	JellyfinItems *			m_pSearchResults;

	static LRESULT CALLBACK	ImportDlgProcStub(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK		ImportDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void					SetImportBusy(HWND hDlg, bool bBusy, LPCTSTR szStatus);
	void					SearchAndShow(HWND hDlg);
	void					AddSelected(HWND hDlg);

	// GET <active server><szEndpoint>?<szQuery> for an { Items: [...] } reply, e.g. /Items or /Artists
	JellyfinResult			GetItems(LPCTSTR szEndpoint, LPCTSTR szQuery, JellyfinItems * pItems, LPCTSTR szAction);
	void					AppendArtistAlbums(LPCTSTR szEncodedTerm, JellyfinItems * pItems);
	bool					AddItemTracks(const rapidjson::Value & item, TrackList * pTracks);

public:
	CJellyfinService();
	~CJellyfinService();

	void				Destroy(void);

	LPTSTR				GetPluginName(void);
	unsigned long		GetFlags(void);

	bool				SetHelper(ITuniacServicePluginHelper* pHelper);

	bool				About(HWND hWndParent);
	bool				Configure(HWND hWndParent);

	bool				CanHandle(LPCTSTR szSource);
	bool				TranslateSource(LPTSTR szDest, size_t cchDest, LPCTSTR szSource);
	void				SourceFailed(LPCTSTR szSource);

	bool				Import(HWND hWndParent);

	// Authenticate against a Jellyfin server (e.g. "https://jellyfin.example.com") and make it the
	// active server. The token, user and server URL are persisted per ServerId; the password is not.
	JellyfinResult		Login(LPCTSTR szServerUrl, LPCTSTR szUsername, LPCTSTR szPassword);

	// Check that a Jellyfin server answers at szServerUrl, without logging in. On success szInfo
	// describes the server.
	JellyfinResult		TestConnection(LPCTSTR szServerUrl, LPTSTR szInfo, size_t cchInfo);

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
