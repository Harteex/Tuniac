#include "stdafx.h"
#include "JellyfinService.h"
#include "HttpService.h"
#include "resource.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <objbase.h>
#include <tchar.h>

static HINSTANCE g_hInstance = NULL;

// Caller delete[]s the result
static char* TcharToUtf8(LPCTSTR szSrc)
{
	int iLen = WideCharToMultiByte(CP_UTF8, 0, szSrc, -1, NULL, 0, NULL, NULL);
	if (iLen <= 0)
		return NULL;

	char* szUtf8 = new char[iLen];
	WideCharToMultiByte(CP_UTF8, 0, szSrc, -1, szUtf8, iLen, NULL, NULL);
	return szUtf8;
}

static bool GetJsonString(const rapidjson::Value& obj, const char* szName, LPTSTR szDest, size_t cchDest)
{
	szDest[0] = TEXT('\0');
	if (!obj.IsObject() || !obj.HasMember(szName) || !obj[szName].IsString())
		return false;

	return MultiByteToWideChar(CP_UTF8, 0, obj[szName].GetString(), -1, szDest, (int)cchDest) > 0;
}

// Percent-encode as UTF-8, for values in the Authorization header
static bool UrlEncode(LPCTSTR szSrc, LPTSTR szDest, size_t cchDest)
{
	char* szUtf8 = TcharToUtf8(szSrc);
	if (szUtf8 == NULL)
		return false;

	bool bOK = true;
	size_t iPos = 0;
	for (unsigned char* p = (unsigned char*)szUtf8; *p; p++)
	{
		bool bPlain = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
			*p == '-' || *p == '_' || *p == '.' || *p == '~';

		if (iPos + (bPlain ? 1 : 3) >= cchDest)
		{
			bOK = false;
			break;
		}

		if (bPlain)
		{
			szDest[iPos++] = (TCHAR)*p;
		}
		else
		{
			StringCchPrintf(&szDest[iPos], cchDest - iPos, TEXT("%%%02X"), *p);
			iPos += 3;
		}
	}
	szDest[iPos] = TEXT('\0');

	delete[] szUtf8;
	return bOK;
}

// Trim whitespace and trailing slashes (which would turn request paths into "//Users/...")
// and require an http:// or https:// scheme with something after it
static bool NormalizeServerUrl(LPCTSTR szSrc, LPTSTR szDest, size_t cchDest)
{
	szDest[0] = TEXT('\0');
	if (szSrc == NULL)
		return false;

	while (*szSrc == TEXT(' ') || *szSrc == TEXT('\t'))
		szSrc++;

	if (FAILED(StringCchCopy(szDest, cchDest, szSrc)))
	{
		szDest[0] = TEXT('\0');
		return false;
	}

	size_t iLen = _tcslen(szDest);
	while (iLen > 0 && (szDest[iLen - 1] == TEXT('/') || szDest[iLen - 1] == TEXT(' ') || szDest[iLen - 1] == TEXT('\t')))
		szDest[--iLen] = TEXT('\0');

	if (StrCmpNI(szDest, TEXT("http://"), 7) == 0)
		return iLen > 7;
	if (StrCmpNI(szDest, TEXT("https://"), 8) == 0)
		return iLen > 8;
	return false;
}

// ServerIds become registry key names, so only accept plain ids
static bool IsSafeId(LPCTSTR szId)
{
	if (szId == NULL || szId[0] == TEXT('\0') || _tcslen(szId) >= 64)
		return false;

	for (LPCTSTR p = szId; *p; p++)
	{
		if (!((*p >= TEXT('a') && *p <= TEXT('z')) || (*p >= TEXT('A') && *p <= TEXT('Z')) ||
			(*p >= TEXT('0') && *p <= TEXT('9')) || *p == TEXT('-')))
			return false;
	}
	return true;
}

BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD  ulReason,
	LPVOID lpReserved
)
{
	if (ulReason == DLL_PROCESS_ATTACH)
		g_hInstance = (HINSTANCE)hModule;

	return TRUE;
}

extern "C" __declspec(dllexport) ITuniacServicePlugin * CreateTuniacServicePlugin(void)
{
	ITuniacServicePlugin* pPlugin = new CJellyfinService;
	return pPlugin;
}

extern "C" __declspec(dllexport) unsigned long		GetTuniacServicePluginVersion(void)
{
	return ITUNIACSERVICEPLUGIN_VERSION;
}


CJellyfinService::CJellyfinService() :
	m_pHelper(NULL),
	m_bAllowInvalidCertificates(false)
{
	m_szDeviceId[0] = TEXT('\0');
	m_szDeviceName[0] = TEXT('\0');
	m_szServerId[0] = TEXT('\0');
	m_szServerUrl[0] = TEXT('\0');
	m_szUserId[0] = TEXT('\0');
	m_szUserName[0] = TEXT('\0');
	m_szToken[0] = TEXT('\0');
	m_szLastError[0] = TEXT('\0');
}

CJellyfinService::~CJellyfinService()
{

}

void			CJellyfinService::Destroy(void)
{
	delete this;
}

LPTSTR			CJellyfinService::GetPluginName(void)
{
	return TEXT("Jellyfin");
}

unsigned long	CJellyfinService::GetFlags(void)
{
	return SERVICEPLUGINFLAGS_ABOUT | SERVICEPLUGINFLAGS_CONFIG;
}

bool			CJellyfinService::SetHelper(ITuniacServicePluginHelper* pHelper)
{
	m_pHelper = pHelper;
	LoadSettings();
	return true;
}

// config dialog

// Posted from WM_INITDIALOG so the stored login is checked after the dialog is visible
#define WM_JELLYFIN_VALIDATE		(WM_APP + 1)

LRESULT CALLBACK	CJellyfinService::DlgProcStub(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG)
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);

	// Some messages (WM_SETFONT) arrive before WM_INITDIALOG
	CJellyfinService* pJellyfin = (CJellyfinService*)(LONG_PTR)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	if (pJellyfin == NULL)
		return FALSE;

	return pJellyfin->DlgProc(hDlg, uMsg, wParam, lParam);
}

void			CJellyfinService::SetDialogBusy(HWND hDlg, LPCTSTR szStatus)
{
	EnableWindow(GetDlgItem(hDlg, IDC_JF_TEST), FALSE);
	EnableWindow(GetDlgItem(hDlg, IDC_JF_LOGIN), FALSE);
	EnableWindow(GetDlgItem(hDlg, IDC_JF_LOGOUT), FALSE);
	SetDlgItemText(hDlg, IDC_JF_STATUS, szStatus);

	// Requests block this thread, so paint now and show a wait cursor until they return
	UpdateWindow(hDlg);
	SetCursor(LoadCursor(NULL, IDC_WAIT));
}

void			CJellyfinService::UpdateDialog(HWND hDlg, LPCTSTR szNote)
{
	bool bLoggedIn = IsLoggedIn();

	// Read-only rather than disabled, so the URL can still be selected and copied
	SendDlgItemMessage(hDlg, IDC_JF_SERVERURL, EM_SETREADONLY, bLoggedIn, 0);
	SendDlgItemMessage(hDlg, IDC_JF_USERNAME, EM_SETREADONLY, bLoggedIn, 0);
	EnableWindow(GetDlgItem(hDlg, IDC_JF_PASSWORD), !bLoggedIn);
	EnableWindow(GetDlgItem(hDlg, IDC_JF_TEST), !bLoggedIn);
	EnableWindow(GetDlgItem(hDlg, IDC_JF_LOGIN), !bLoggedIn);
	EnableWindow(GetDlgItem(hDlg, IDC_JF_LOGOUT), bLoggedIn);

	TCHAR szStatus[1024];
	if (bLoggedIn)
		StringCchPrintf(szStatus, 1024, TEXT("Logged in as %s on %s."), m_szUserName, m_szServerUrl);
	else
		StringCchCopy(szStatus, 1024, TEXT("Not logged in."));

	if (szNote != NULL && szNote[0] != TEXT('\0'))
	{
		StringCchCat(szStatus, 1024, TEXT("\r\n\r\n"));
		StringCchCat(szStatus, 1024, szNote);
	}
	if (m_szLastError[0] != TEXT('\0'))
	{
		StringCchCat(szStatus, 1024, TEXT("\r\n\r\n"));
		StringCchCat(szStatus, 1024, m_szLastError);
	}

	SetDlgItemText(hDlg, IDC_JF_STATUS, szStatus);
}

LRESULT CALLBACK	CJellyfinService::DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			m_szLastError[0] = TEXT('\0');

			SendDlgItemMessage(hDlg, IDC_JF_SERVERURL, EM_LIMITTEXT, 255, 0);
			SendDlgItemMessage(hDlg, IDC_JF_USERNAME, EM_LIMITTEXT, 127, 0);
			SendDlgItemMessage(hDlg, IDC_JF_PASSWORD, EM_LIMITTEXT, 255, 0);

			SetDlgItemText(hDlg, IDC_JF_SERVERURL, m_szServerUrl);
			SetDlgItemText(hDlg, IDC_JF_USERNAME, m_szUserName);
			CheckDlgButton(hDlg, IDC_JF_ALLOWINVALIDCERT, m_bAllowInvalidCertificates ? BST_CHECKED : BST_UNCHECKED);

			UpdateDialog(hDlg, NULL);

			if (IsLoggedIn())
			{
				PostMessage(hDlg, WM_JELLYFIN_VALIDATE, 0, 0);
			}
			else
			{
				// Start in the first empty field
				int iFocus = m_szServerUrl[0] == TEXT('\0') ? IDC_JF_SERVERURL : (m_szUserName[0] == TEXT('\0') ? IDC_JF_USERNAME : IDC_JF_PASSWORD);
				SetFocus(GetDlgItem(hDlg, iFocus));
				return FALSE;
			}
		}
		break;

		case WM_JELLYFIN_VALIDATE:
		{
			SetDialogBusy(hDlg, TEXT("Checking login..."));
			ValidateSession();
			UpdateDialog(hDlg, NULL);
		}
		break;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_JF_ALLOWINVALIDCERT:
				{
					SetAllowInvalidCertificates(IsDlgButtonChecked(hDlg, IDC_JF_ALLOWINVALIDCERT) == BST_CHECKED);
				}
				break;

				case IDC_JF_TEST:
				{
					TCHAR szUrl[256];
					TCHAR szInfo[256];
					GetDlgItemText(hDlg, IDC_JF_SERVERURL, szUrl, 256);

					SetDialogBusy(hDlg, TEXT("Connecting..."));
					TestConnection(szUrl, szInfo, 256);
					UpdateDialog(hDlg, szInfo);
				}
				break;

				case IDC_JF_LOGIN:
				{
					// Login is the default button, so Enter can arrive here while logged in
					if (IsLoggedIn())
						break;

					TCHAR szUrl[256];
					TCHAR szUsername[128];
					TCHAR szPassword[256];
					GetDlgItemText(hDlg, IDC_JF_SERVERURL, szUrl, 256);
					GetDlgItemText(hDlg, IDC_JF_USERNAME, szUsername, 128);
					GetDlgItemText(hDlg, IDC_JF_PASSWORD, szPassword, 256);

					SetDialogBusy(hDlg, TEXT("Logging in..."));
					JellyfinResult result = Login(szUrl, szUsername, szPassword);
					SecureZeroMemory(szPassword, sizeof(szPassword));

					if (result == JELLYFIN_OK)
					{
						// Show the stored, normalised URL
						SetDlgItemText(hDlg, IDC_JF_SERVERURL, m_szServerUrl);
						SetDlgItemText(hDlg, IDC_JF_PASSWORD, TEXT(""));
					}

					UpdateDialog(hDlg, NULL);
					SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IsLoggedIn() ? IDC_JF_LOGOUT : IDC_JF_PASSWORD), TRUE);
				}
				break;

				case IDC_JF_LOGOUT:
				{
					SetDialogBusy(hDlg, TEXT("Logging out..."));
					Logout();
					UpdateDialog(hDlg, NULL);
					SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDC_JF_PASSWORD), TRUE);
				}
				break;

				case IDOK:
				case IDCANCEL:
				{
					EndDialog(hDlg, wParam);
				}
				break;

				default:
					return FALSE;
			}
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

bool			CJellyfinService::About(HWND hWndParent)
{
	MessageBox(hWndParent, TEXT("Jellyfin service plugin for Tuniac.\r\nBy Harteex, 2022.\r\n\r\nThis plugin enables streaming music from a Jellyfin server."), TEXT("About"), MB_OK | MB_ICONINFORMATION);
	return true;
}

bool			CJellyfinService::Configure(HWND hWndParent)
{
	DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_JELLYFIN_CONFIG), hWndParent, (DLGPROC)DlgProcStub, (DWORD_PTR)this);

	return true;
}

void			CJellyfinService::SetAllowInvalidCertificates(bool bAllow)
{
	m_bAllowInvalidCertificates = bAllow;

	if (m_pHelper)
	{
		DWORD dwValue = bAllow ? 1 : 0;
		m_pHelper->PreferencesSet(JELLYFIN_PREFS_KEY, TEXT("AllowInvalidCertificates"), REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
	}
}

// settings

bool			CJellyfinService::GetPrefString(LPCTSTR szSubKey, LPCTSTR szName, LPTSTR szDest, size_t cchDest)
{
	szDest[0] = TEXT('\0');
	if (m_pHelper == NULL)
		return false;

	// Leave room for a terminator; registry strings aren't guaranteed to have one
	DWORD dwType = REG_SZ;
	DWORD cbData = (DWORD)((cchDest - 1) * sizeof(TCHAR));
	if (!m_pHelper->PreferencesGet(szSubKey, szName, &dwType, (LPBYTE)szDest, &cbData) || dwType != REG_SZ)
	{
		szDest[0] = TEXT('\0');
		return false;
	}

	szDest[cbData / sizeof(TCHAR)] = TEXT('\0');
	return true;
}

bool			CJellyfinService::SetPrefString(LPCTSTR szSubKey, LPCTSTR szName, LPCTSTR szValue)
{
	if (m_pHelper == NULL)
		return false;

	return m_pHelper->PreferencesSet(szSubKey, szName, REG_SZ, (const BYTE*)szValue, (DWORD)((_tcslen(szValue) + 1) * sizeof(TCHAR)));
}

void			CJellyfinService::LoadSettings(void)
{
	// The server ties sessions to DeviceId, so it must be unique to this install and never change
	if (!GetPrefString(JELLYFIN_PREFS_KEY, TEXT("DeviceId"), m_szDeviceId, 64) || m_szDeviceId[0] == TEXT('\0'))
	{
		GUID guid;
		if (SUCCEEDED(CoCreateGuid(&guid)))
		{
			StringCchPrintf(m_szDeviceId, 64, TEXT("%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x"),
				guid.Data1, guid.Data2, guid.Data3,
				guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
				guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
			SetPrefString(JELLYFIN_PREFS_KEY, TEXT("DeviceId"), m_szDeviceId);
		}
	}

	DWORD cchDeviceName = 128;
	if (!GetComputerName(m_szDeviceName, &cchDeviceName))
		StringCchCopy(m_szDeviceName, 128, TEXT("Windows"));

	DWORD dwType = REG_DWORD;
	DWORD dwValue = 0;
	DWORD cbData = sizeof(DWORD);
	if (m_pHelper->PreferencesGet(JELLYFIN_PREFS_KEY, TEXT("AllowInvalidCertificates"), &dwType, (LPBYTE)&dwValue, &cbData) && dwType == REG_DWORD)
		m_bAllowInvalidCertificates = dwValue != 0;

	TCHAR szActiveServer[64];
	if (GetPrefString(JELLYFIN_PREFS_KEY, TEXT("ActiveServer"), szActiveServer, 64))
		LoadServer(szActiveServer);
}

void			CJellyfinService::LoadServer(LPCTSTR szServerId)
{
	if (!IsSafeId(szServerId))
		return;

	TCHAR szSubKey[128];
	StringCchPrintf(szSubKey, 128, TEXT("%s\\Servers\\%s"), JELLYFIN_PREFS_KEY, szServerId);

	StringCchCopy(m_szServerId, 64, szServerId);
	GetPrefString(szSubKey, TEXT("Url"), m_szServerUrl, 256);
	GetPrefString(szSubKey, TEXT("UserId"), m_szUserId, 64);
	GetPrefString(szSubKey, TEXT("UserName"), m_szUserName, 128);
	GetPrefString(szSubKey, TEXT("Token"), m_szToken, 128);
}

void			CJellyfinService::SaveServer(void)
{
	if (!IsSafeId(m_szServerId))
		return;

	TCHAR szSubKey[128];
	StringCchPrintf(szSubKey, 128, TEXT("%s\\Servers\\%s"), JELLYFIN_PREFS_KEY, m_szServerId);

	SetPrefString(szSubKey, TEXT("Url"), m_szServerUrl);
	SetPrefString(szSubKey, TEXT("UserId"), m_szUserId);
	SetPrefString(szSubKey, TEXT("UserName"), m_szUserName);
	SetPrefString(szSubKey, TEXT("Token"), m_szToken);

	SetPrefString(JELLYFIN_PREFS_KEY, TEXT("ActiveServer"), m_szServerId);
}

// The stored login for any server, not just the active one, so links to every server we've
// logged in to keep playing
bool			CJellyfinService::GetServerLogin(LPCTSTR szServerId, LPTSTR szUrl, size_t cchUrl, LPTSTR szToken, size_t cchToken)
{
	szUrl[0] = TEXT('\0');
	szToken[0] = TEXT('\0');

	if (!IsSafeId(szServerId))
		return false;

	if (StrCmpI(szServerId, m_szServerId) == 0)
	{
		StringCchCopy(szUrl, cchUrl, m_szServerUrl);
		StringCchCopy(szToken, cchToken, m_szToken);
	}
	else
	{
		TCHAR szSubKey[128];
		StringCchPrintf(szSubKey, 128, TEXT("%s\\Servers\\%s"), JELLYFIN_PREFS_KEY, szServerId);
		GetPrefString(szSubKey, TEXT("Url"), szUrl, cchUrl);
		GetPrefString(szSubKey, TEXT("Token"), szToken, cchToken);
	}

	return szUrl[0] != TEXT('\0') && szToken[0] != TEXT('\0');
}

// playback

// jellyfin://<ServerId>/<ItemId>; both buffers 64 characters
static bool ParseLink(LPCTSTR szSource, LPTSTR szServerId, LPTSTR szItemId)
{
	szServerId[0] = TEXT('\0');
	szItemId[0] = TEXT('\0');

	if (szSource == NULL || StrCmpNI(szSource, JELLYFIN_SCHEME, JELLYFIN_SCHEME_LEN) != 0)
		return false;

	LPCTSTR szIds = szSource + JELLYFIN_SCHEME_LEN;
	LPCTSTR szSlash = StrChr(szIds, TEXT('/'));

	return szSlash != NULL &&
		SUCCEEDED(StringCchCopyN(szServerId, 64, szIds, szSlash - szIds)) &&
		SUCCEEDED(StringCchCopy(szItemId, 64, szSlash + 1)) &&
		IsSafeId(szServerId) && IsSafeId(szItemId);
}

void			CJellyfinService::SourceFailed(LPCTSTR szSource)
{
	TCHAR szServerId[64];
	TCHAR szItemId[64];
	TCHAR szUrl[256];
	TCHAR szToken[128];
	if (!ParseLink(szSource, szServerId, szItemId) || !GetServerLogin(szServerId, szUrl, 256, szToken, 128))
		return;

	// Ask the server what the item is; the decoder can only say that opening the stream failed
	TCHAR szPath[256];
	TCHAR szRequestUrl[512];
	TCHAR szHeaders[1024];
	StringCchPrintf(szPath, 256, TEXT("/Items?Ids=%s&Fields=MediaSources"), szItemId);
	if (!BuildUrl(szRequestUrl, 512, szUrl, szPath) || !BuildAuthHeader(szHeaders, 1024, szToken))
		return;

	CHttpService httpService;
	httpService.SetAllowInvalidCertificates(m_bAllowInvalidCertificates);
	HttpResponse response = httpService.Get(szRequestUrl, szHeaders);

	TCHAR szAction[512];
	StringCchPrintf(szAction, 512, TEXT("Playing %s"), szSource);
	JellyfinResult result = ResultFromResponse(response, szAction);

	if (result == JELLYFIN_ERROR_UNAUTHORIZED)
	{
		SetError(TEXT("Can't play %s: the login to %s has expired or was revoked. Log in again from the plugin's Configure dialog."), szSource, szUrl);
	}
	else if (result == JELLYFIN_OK)
	{
		rapidjson::Document responseDoc;
		if (response.pContent != NULL)
			responseDoc.Parse(response.pContent);

		if (response.pContent == NULL || responseDoc.HasParseError() || !responseDoc.IsObject() ||
			!responseDoc.HasMember("Items") || !responseDoc["Items"].IsArray())
		{
			SetError(TEXT("Can't play %s: unexpected reply from the server."), szSource);
		}
		else if (responseDoc["Items"].Size() == 0)
		{
			SetError(TEXT("Can't play %s: the server has no item with that id."), szSource);
		}
		else
		{
			const rapidjson::Value& item = responseDoc["Items"][0];
			TCHAR szType[64];
			TCHAR szName[128];
			GetJsonString(item, "Type", szType, 64);
			GetJsonString(item, "Name", szName, 128);

			if (StrCmpI(szType, TEXT("Audio")) != 0)
			{
				SetError(TEXT("Can't play %s: it is a %s (\"%s\"), not a track. Links must point to a single track."), szSource, szType, szName);
			}
			else
			{
				TCHAR szContainer[32] = TEXT("unknown");
				if (item.HasMember("MediaSources") && item["MediaSources"].IsArray() && item["MediaSources"].Size() > 0)
					GetJsonString(item["MediaSources"][0], "Container", szContainer, 32);

				SetError(TEXT("Can't play %s (\"%s\"): the server has the track, but it couldn't be streamed or decoded (format: %s)."), szSource, szName, szContainer);
			}
		}
	}

	if (response.pContent != NULL)
		delete[] response.pContent;
}

bool			CJellyfinService::CanHandle(LPCTSTR szSource)
{
	return szSource != NULL && StrCmpNI(szSource, JELLYFIN_SCHEME, JELLYFIN_SCHEME_LEN) == 0;
}

bool			CJellyfinService::TranslateSource(LPTSTR szDest, size_t cchDest, LPCTSTR szSource)
{
	if (!CanHandle(szSource))
		return false;

	TCHAR szServerId[64];
	TCHAR szItemId[64];
	if (!ParseLink(szSource, szServerId, szItemId))
	{
		SetError(TEXT("Can't play %s: not a valid Jellyfin link."), szSource);
		return false;
	}

	TCHAR szUrl[256];
	TCHAR szToken[128];
	if (!GetServerLogin(szServerId, szUrl, 256, szToken, 128))
	{
		SetError(TEXT("Can't play %s: not logged in to that Jellyfin server."), szSource);
		return false;
	}

	// static=true streams the original file, which the decoder must be able to play. The auth
	// header goes after the URL separated by \r\n, which BASS_StreamCreateURL sends as request headers.
	TCHAR szPath[128];
	TCHAR szHeaders[1024];
	StringCchPrintf(szPath, 128, TEXT("/Audio/%s/stream?static=true"), szItemId);
	if (!BuildUrl(szDest, cchDest, szUrl, szPath) ||
		!BuildAuthHeader(szHeaders, 1024, szToken) ||
		FAILED(StringCchCat(szDest, cchDest, TEXT("\r\n"))) ||
		FAILED(StringCchCat(szDest, cchDest, szHeaders)))
	{
		SetError(TEXT("Can't play %s: could not build the stream request."), szSource);
		return false;
	}

	return true;
}

// requests

bool			CJellyfinService::BuildAuthHeader(LPTSTR szDest, size_t cchDest, LPCTSTR szToken)
{
	TCHAR szDevice[384];
	if (!UrlEncode(m_szDeviceName, szDevice, 384))
		return false;

	HRESULT hr;
	if (szToken != NULL && szToken[0] != TEXT('\0'))
	{
		hr = StringCchPrintf(szDest, cchDest,
			TEXT("Authorization: MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\", Token=\"%s\"\r\n"),
			JELLYFIN_CLIENT_NAME, szDevice, m_szDeviceId, JELLYFIN_CLIENT_VERSION, szToken);
	}
	else
	{
		hr = StringCchPrintf(szDest, cchDest,
			TEXT("Authorization: MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\"\r\n"),
			JELLYFIN_CLIENT_NAME, szDevice, m_szDeviceId, JELLYFIN_CLIENT_VERSION);
	}

	return SUCCEEDED(hr);
}

bool			CJellyfinService::BuildUrl(LPTSTR szDest, size_t cchDest, LPCTSTR szServerUrl, LPCTSTR szPath)
{
	return SUCCEEDED(StringCchPrintf(szDest, cchDest, TEXT("%s%s"), szServerUrl, szPath));
}

void			CJellyfinService::SetError(LPCTSTR szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	StringCchVPrintf(m_szLastError, 512, szFormat, args);
	va_end(args);

	if (m_pHelper)
		m_pHelper->LogMessage((LPTSTR)TEXT("Jellyfin"), m_szLastError);
}

JellyfinResult	CJellyfinService::ResultFromResponse(HttpResponse& response, LPCTSTR szAction)
{
	if (response.success)
		return JELLYFIN_OK;

	DWORD dwError = response.errorCode;

	if (dwError == ERROR_INTERNET_INVALID_CA ||
		dwError == ERROR_INTERNET_SEC_CERT_CN_INVALID ||
		dwError == ERROR_INTERNET_SEC_CERT_DATE_INVALID ||
		dwError == ERROR_INTERNET_SEC_CERT_ERRORS ||
		dwError == ERROR_INTERNET_SEC_INVALID_CERT ||
		dwError == ERROR_INTERNET_SEC_CERT_REV_FAILED)
	{
		SetError(TEXT("%s failed: the server's HTTPS certificate is not trusted (error %lu)."), szAction, dwError);
		return JELLYFIN_ERROR_CERTIFICATE;
	}

	// No HTTP status, or the connection broke while reading the reply
	if (response.statusCode == 0 || (response.statusCode >= 200 && response.statusCode < 300))
	{
		SetError(TEXT("%s failed: could not reach the server (error %lu)."), szAction, dwError);
		return JELLYFIN_ERROR_CONNECT;
	}

	if (response.statusCode == 401 || response.statusCode == 403)
	{
		SetError(TEXT("%s failed: not authorized (HTTP %lu)."), szAction, response.statusCode);
		return JELLYFIN_ERROR_UNAUTHORIZED;
	}

	SetError(TEXT("%s failed: the server returned HTTP %lu."), szAction, response.statusCode);
	return JELLYFIN_ERROR_RESPONSE;
}

// session

JellyfinResult	CJellyfinService::TestConnection(LPCTSTR szServerUrl, LPTSTR szInfo, size_t cchInfo)
{
	m_szLastError[0] = TEXT('\0');
	szInfo[0] = TEXT('\0');

	TCHAR szUrl[256];
	if (!NormalizeServerUrl(szServerUrl, szUrl, 256))
	{
		SetError(TEXT("The server URL must start with http:// or https://."));
		return JELLYFIN_ERROR_INVALIDARGS;
	}

	TCHAR szRequestUrl[512];
	TCHAR szHeaders[1024];
	if (!BuildUrl(szRequestUrl, 512, szUrl, TEXT("/System/Info/Public")) || !BuildAuthHeader(szHeaders, 1024, NULL))
	{
		SetError(TEXT("Connection test failed: could not build the request."));
		return JELLYFIN_ERROR_INVALIDARGS;
	}

	CHttpService httpService;
	httpService.SetAllowInvalidCertificates(m_bAllowInvalidCertificates);
	HttpResponse response = httpService.Get(szRequestUrl, szHeaders);

	JellyfinResult result = ResultFromResponse(response, TEXT("Connection test"));

	// Any Jellyfin server has this endpoint, so a 404 means something else is answering
	if (result == JELLYFIN_ERROR_RESPONSE && response.statusCode == 404)
		SetError(TEXT("The server answered, but it doesn't look like a Jellyfin server (HTTP 404). Check the URL, including any path such as /jellyfin."));

	if (result == JELLYFIN_OK)
	{
		TCHAR szServerName[128];
		TCHAR szVersion[64];

		rapidjson::Document responseDoc;
		if (response.pContent != NULL)
			responseDoc.Parse(response.pContent);

		if (response.pContent != NULL && !responseDoc.HasParseError() && responseDoc.IsObject() &&
			GetJsonString(responseDoc, "ServerName", szServerName, 128) &&
			GetJsonString(responseDoc, "Version", szVersion, 64))
		{
			StringCchPrintf(szInfo, cchInfo, TEXT("Found Jellyfin server \"%s\" (version %s)."), szServerName, szVersion);
		}
		else
		{
			SetError(TEXT("The server answered, but it doesn't look like a Jellyfin server."));
			result = JELLYFIN_ERROR_RESPONSE;
		}
	}

	if (response.pContent != NULL)
		delete[] response.pContent;

	return result;
}

JellyfinResult	CJellyfinService::Login(LPCTSTR szServerUrl, LPCTSTR szUsername, LPCTSTR szPassword)
{
	m_szLastError[0] = TEXT('\0');

	if (szServerUrl == NULL || szUsername == NULL || szPassword == NULL || szServerUrl[0] == TEXT('\0') || szUsername[0] == TEXT('\0'))
	{
		SetError(TEXT("Login failed: server URL and username are required."));
		return JELLYFIN_ERROR_INVALIDARGS;
	}

	TCHAR szUrl[256];
	if (!NormalizeServerUrl(szServerUrl, szUrl, 256))
	{
		SetError(TEXT("Login failed: the server URL must start with http:// or https://."));
		return JELLYFIN_ERROR_INVALIDARGS;
	}

	// Build the request body as UTF-8 and send those bytes as-is
	char* szUsernameUtf8 = TcharToUtf8(szUsername);
	char* szPasswordUtf8 = TcharToUtf8(szPassword);
	if (szUsernameUtf8 == NULL || szPasswordUtf8 == NULL)
	{
		if (szUsernameUtf8) delete[] szUsernameUtf8;
		if (szPasswordUtf8) delete[] szPasswordUtf8;
		SetError(TEXT("Login failed: could not encode the username or password."));
		return JELLYFIN_ERROR_INVALIDARGS;
	}

	rapidjson::Document doc;
	doc.SetObject();
	rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
	doc.AddMember("Username", rapidjson::Value(szUsernameUtf8, allocator), allocator);
	doc.AddMember("Pw", rapidjson::Value(szPasswordUtf8, allocator), allocator);

	delete[] szUsernameUtf8;
	delete[] szPasswordUtf8;

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	TCHAR szLoginUrl[512];
	TCHAR szHeaders[1024];
	if (!BuildUrl(szLoginUrl, 512, szUrl, TEXT("/Users/AuthenticateByName")) ||
		!BuildAuthHeader(szHeaders, 1024, NULL) ||
		FAILED(StringCchCat(szHeaders, 1024, TEXT("Content-Type: application/json\r\n"))))
	{
		SetError(TEXT("Login failed: could not build the request."));
		return JELLYFIN_ERROR_INVALIDARGS;
	}

	CHttpService httpService;
	httpService.SetAllowInvalidCertificates(m_bAllowInvalidCertificates);
	HttpResponse response = httpService.Post(szLoginUrl, buffer.GetString(), (DWORD)buffer.GetSize(), szHeaders);

	JellyfinResult result = ResultFromResponse(response, TEXT("Login"));
	if (result == JELLYFIN_ERROR_UNAUTHORIZED)
		SetError(TEXT("Login failed: wrong username or password."));

	TCHAR szToken[128];
	TCHAR szServerId[64];
	TCHAR szUserId[64];
	TCHAR szUserName[128];

	if (result == JELLYFIN_OK)
	{
		rapidjson::Document responseDoc;
		if (response.pContent != NULL)
			responseDoc.Parse(response.pContent);

		if (response.pContent == NULL || responseDoc.HasParseError() || !responseDoc.IsObject() ||
			!GetJsonString(responseDoc, "AccessToken", szToken, 128) ||
			!GetJsonString(responseDoc, "ServerId", szServerId, 64) ||
			!responseDoc.HasMember("User") ||
			!GetJsonString(responseDoc["User"], "Id", szUserId, 64) ||
			!GetJsonString(responseDoc["User"], "Name", szUserName, 128) ||
			!IsSafeId(szServerId))
		{
			SetError(TEXT("Login failed: unexpected reply from the server."));
			result = JELLYFIN_ERROR_RESPONSE;
		}
	}

	if (response.pContent != NULL)
		delete[] response.pContent;

	if (result != JELLYFIN_OK)
		return result;

	// Load what we already know about this server first, then overwrite with the new session
	LoadServer(szServerId);
	StringCchCopy(m_szServerUrl, 256, szUrl);
	StringCchCopy(m_szUserId, 64, szUserId);
	StringCchCopy(m_szUserName, 128, szUserName);
	StringCchCopy(m_szToken, 128, szToken);
	SaveServer();

	TCHAR szMessage[512];
	StringCchPrintf(szMessage, 512, TEXT("Logged in to %s as %s."), m_szServerUrl, m_szUserName);
	m_pHelper->LogMessage((LPTSTR)TEXT("Jellyfin"), szMessage);

	return JELLYFIN_OK;
}

JellyfinResult	CJellyfinService::ValidateSession(void)
{
	m_szLastError[0] = TEXT('\0');

	if (!IsLoggedIn())
	{
		SetError(TEXT("Not logged in."));
		return JELLYFIN_ERROR_UNAUTHORIZED;
	}

	TCHAR szUrl[512];
	TCHAR szHeaders[1024];
	if (!BuildUrl(szUrl, 512, m_szServerUrl, TEXT("/Users/Me")) || !BuildAuthHeader(szHeaders, 1024, m_szToken))
	{
		SetError(TEXT("Checking login failed: could not build the request."));
		return JELLYFIN_ERROR_INVALIDARGS;
	}

	CHttpService httpService;
	httpService.SetAllowInvalidCertificates(m_bAllowInvalidCertificates);
	HttpResponse response = httpService.Get(szUrl, szHeaders);

	JellyfinResult result = ResultFromResponse(response, TEXT("Checking login"));

	if (response.pContent != NULL)
		delete[] response.pContent;

	if (result == JELLYFIN_ERROR_UNAUTHORIZED)
	{
		m_szToken[0] = TEXT('\0');
		SaveServer();
		SetError(TEXT("The login to %s has expired or was revoked. Please log in again."), m_szServerUrl);
	}

	return result;
}

JellyfinResult	CJellyfinService::Logout(void)
{
	m_szLastError[0] = TEXT('\0');

	if (!IsLoggedIn())
		return JELLYFIN_OK;

	TCHAR szUrl[512];
	TCHAR szHeaders[1024];
	if (BuildUrl(szUrl, 512, m_szServerUrl, TEXT("/Sessions/Logout")) && BuildAuthHeader(szHeaders, 1024, m_szToken))
	{
		CHttpService httpService;
		httpService.SetAllowInvalidCertificates(m_bAllowInvalidCertificates);
		HttpResponse response = httpService.Post(szUrl, NULL, 0, szHeaders);

		// Only logged; if the server couldn't be told, the token stays valid there until it's revoked
		ResultFromResponse(response, TEXT("Logout"));

		if (response.pContent != NULL)
			delete[] response.pContent;
	}

	m_szToken[0] = TEXT('\0');
	SaveServer();

	TCHAR szMessage[512];
	StringCchPrintf(szMessage, 512, TEXT("Logged out from %s."), m_szServerUrl);
	m_pHelper->LogMessage((LPTSTR)TEXT("Jellyfin"), szMessage);

	return JELLYFIN_OK;
}
