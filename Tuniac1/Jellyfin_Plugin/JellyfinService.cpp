#include "stdafx.h"
#include "JellyfinService.h"
#include "HttpService.h"
#include "resource.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD  ulReason,
	LPVOID lpReserved
)
{
	return TRUE;
}

extern "C" __declspec(dllexport) ITuniacServicePlugin * CreateTuniacServicePlugin(void)
{
	ITuniacServicePlugin* pPlugin = new CJellyfinService;
	return pPlugin;
}

extern "C" __declspec(dllexport) unsigned long		GetTuniacPluginVersion(void)
{
	return ITUNIACSERVICEPLUGIN_VERSION;
}


CJellyfinService::CJellyfinService()
{

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
	return true;
}

//pref dialog
LRESULT CALLBACK	CJellyfinService::DlgProcStub(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG)
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);

	CJellyfinService* pPopupNotify = (CJellyfinService*)(LONG_PTR)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	return(pPopupNotify->DlgProc(hDlg, uMsg, wParam, lParam));
}

LRESULT CALLBACK	CJellyfinService::DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CJellyfinService* pPopupNotify = (CJellyfinService*)(LONG_PTR)GetWindowLongPtr(hDlg, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
			pPopupNotify = (CJellyfinService*)lParam;

			//SendDlgItemMessage(hDlg, IDC_FULLSCREENINHIBIT_CHECK, BM_SETCHECK, /*pPopupNotify->m_bAllowInhibit*/true ? BST_CHECKED : BST_UNCHECKED, 0);

			//SendDlgItemMessage(hDlg, IDC_TRIGGER_MANUAL, BM_SETCHECK, /*pPopupNotify->m_bManualTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);
			//SendDlgItemMessage(hDlg, IDC_TRIGGER_MANUALBLIND, BM_SETCHECK, /*pPopupNotify->m_bManualBlindTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);
			//SendDlgItemMessage(hDlg, IDC_TRIGGER_AUTO, BM_SETCHECK, /*pPopupNotify->m_bAutoTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);
			//SendDlgItemMessage(hDlg, IDC_TRIGGER_AUTOBLIND, BM_SETCHECK, /*pPopupNotify->m_bAutoBlindTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);


			//SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, CB_RESETCONTENT, 0, 0);
			//SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, WM_SETTEXT, 0, (LPARAM)/*pPopupNotify->m_szNormalFormatString*/TEXT("fixme"));
			//SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@T - @A"));
			//SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@A - @T"));

			//SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, CB_RESETCONTENT, 0, 0);
			//SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, WM_SETTEXT, 0, (LPARAM)/*pPopupNotify->m_szStreamFormatString*/TEXT("fixme"));
			//SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@T - @A"));
			//SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@A - @T"));


		}
		break;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				/*case IDC_FULLSCREENINHIBIT_CHECK:
				{
					int State = SendDlgItemMessage(hDlg, IDC_FULLSCREENINHIBIT_CHECK, BM_GETCHECK, 0, 0);
					pPopupNotify->m_bAllowInhibit = State == BST_UNCHECKED ? FALSE : TRUE;
					m_pHelper->PreferencesSet(TEXT("PopupNotify"), TEXT("AllowInhibit"), REG_DWORD, (LPBYTE)&m_bAllowInhibit, sizeof(int));
				}
				break;

				case IDC_TRIGGER_MANUAL:
				{
					int State = SendDlgItemMessage(hDlg, IDC_TRIGGER_MANUAL, BM_GETCHECK, 0, 0);
					pPopupNotify->m_bManualTrigger = State == BST_UNCHECKED ? FALSE : TRUE;
					m_pHelper->PreferencesSet(TEXT("PopupNotify"), TEXT("Manual"), REG_DWORD, (LPBYTE)&m_bManualTrigger, sizeof(int));
				}
				break;

				case IDC_TRIGGER_MANUALBLIND:
				{
					int State = SendDlgItemMessage(hDlg, IDC_TRIGGER_MANUALBLIND, BM_GETCHECK, 0, 0);
					pPopupNotify->m_bManualBlindTrigger = State == BST_UNCHECKED ? FALSE : TRUE;
					m_pHelper->PreferencesSet(TEXT("PopupNotify"), TEXT("ManualBlind"), REG_DWORD, (LPBYTE)&m_bManualBlindTrigger, sizeof(int));
				}
				break;

				case IDC_TRIGGER_AUTO:
				{
					int State = SendDlgItemMessage(hDlg, IDC_TRIGGER_AUTO, BM_GETCHECK, 0, 0);
					pPopupNotify->m_bAutoTrigger = State == BST_UNCHECKED ? FALSE : TRUE;
					m_pHelper->PreferencesSet(TEXT("PopupNotify"), TEXT("Auto"), REG_DWORD, (LPBYTE)&m_bAutoTrigger, sizeof(int));
				}
				break;

				case IDC_TRIGGER_AUTOBLIND:
				{
					int State = SendDlgItemMessage(hDlg, IDC_TRIGGER_AUTOBLIND, BM_GETCHECK, 0, 0);
					pPopupNotify->m_bAutoBlindTrigger = State == BST_UNCHECKED ? FALSE : TRUE;
					m_pHelper->PreferencesSet(TEXT("PopupNotify"), TEXT("AutoBlind"), REG_DWORD, (LPBYTE)&m_bAutoBlindTrigger, sizeof(int));
				}
				break;

				case IDC_FORMATTING_NORMALFORMAT:
				{
					SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, WM_GETTEXT, 256, (LPARAM)m_szNormalFormatString);
					m_pHelper->PreferencesSet(TEXT("PopupNotify"), TEXT("NormalFormatString"), REG_DWORD, (LPBYTE)&m_szNormalFormatString, (128 * sizeof(WCHAR)));
				}
				break;

				case IDC_FORMATTING_STREAMFORMAT:
				{
					SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, WM_GETTEXT, 256, (LPARAM)m_szStreamFormatString);
					m_pHelper->PreferencesSet(TEXT("PopupNotify"), TEXT("StreamFormatString"), REG_DWORD, (LPBYTE)&m_szStreamFormatString, (128 * sizeof(WCHAR)));
				}
				break;

				case IDC_FORMATTING_FORMATSTRING_HELP:
				{
					MessageBox(hDlg, FORMATSTRING_HELP, TEXT("Help"), MB_OK | MB_ICONINFORMATION);
				}
				break;*/

				case IDOK:
				case IDCANCEL:
				{
					EndDialog(hDlg, wParam);
					return TRUE;
				}
				break;
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
	DialogBoxParam(GetModuleHandle(L"Jellyfin_Plugin.dll"), MAKEINTRESOURCE(IDD_NOTIFYPREFWINDOW), hWndParent, (DLGPROC)DlgProcStub, (DWORD_PTR)this);

	return true;
}

bool CJellyfinService::Login(LPTSTR szServerUrl, LPTSTR szUsername, LPTSTR szPassword, LPTSTR szTokenOut, size_t tokenSize)
{
	if (szServerUrl == NULL || szUsername == NULL || szPassword == NULL || szTokenOut == NULL || tokenSize == 0)
	{
		return false;
	}

	// Build login URL
	TCHAR szLoginUrl[512];
	HRESULT hr = StringCchPrintf(szLoginUrl, 512, TEXT("%s/Users/AuthenticateByName"), szServerUrl);
	if (FAILED(hr))
	{
		return false;
	}

	// Convert TCHAR strings to UTF-8 for JSON
	int usernameLen = WideCharToMultiByte(CP_UTF8, 0, szUsername, -1, NULL, 0, NULL, NULL);
	int passwordLen = WideCharToMultiByte(CP_UTF8, 0, szPassword, -1, NULL, 0, NULL, NULL);
	
	if (usernameLen <= 0 || passwordLen <= 0)
	{
		return false;
	}

	char* szUsernameUtf8 = new char[usernameLen];
	char* szPasswordUtf8 = new char[passwordLen];
	
	if (szUsernameUtf8 == NULL || szPasswordUtf8 == NULL)
	{
		if (szUsernameUtf8) delete[] szUsernameUtf8;
		if (szPasswordUtf8) delete[] szPasswordUtf8;
		return false;
	}

	WideCharToMultiByte(CP_UTF8, 0, szUsername, -1, szUsernameUtf8, usernameLen, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, szPassword, -1, szPasswordUtf8, passwordLen, NULL, NULL);

	// Create JSON request using RapidJSON
	rapidjson::Document doc;
	doc.SetObject();
	rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
	
	doc.AddMember("Username", rapidjson::Value(szUsernameUtf8, allocator), allocator);
	doc.AddMember("Pw", rapidjson::Value(szPasswordUtf8, allocator), allocator);

	// Convert JSON document to UTF-8 string
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	
	const char* jsonStr = buffer.GetString();
	size_t jsonLen = buffer.GetSize();

	// Convert JSON string to TCHAR for HttpService
	int jsonTcharLen = MultiByteToWideChar(CP_UTF8, 0, jsonStr, (int)jsonLen, NULL, 0);
	if (jsonTcharLen <= 0)
	{
		delete[] szUsernameUtf8;
		delete[] szPasswordUtf8;
		return false;
	}

	TCHAR* szJsonTchar = new TCHAR[jsonTcharLen + 1];
	if (szJsonTchar == NULL)
	{
		delete[] szUsernameUtf8;
		delete[] szPasswordUtf8;
		return false;
	}

	MultiByteToWideChar(CP_UTF8, 0, jsonStr, (int)jsonLen, szJsonTchar, jsonTcharLen);
	szJsonTchar[jsonTcharLen] = TEXT('\0');

	// Clean up UTF-8 buffers
	delete[] szUsernameUtf8;
	delete[] szPasswordUtf8;

	// Prepare headers
	TCHAR szHeaders[256];
	StringCchCopy(szHeaders, 256, TEXT("Content-Type: application/json\r\n"));

	// Create HttpService and perform POST request
	CHttpService httpService;
	HttpResponse response = httpService.Post(szLoginUrl, szJsonTchar, szHeaders);

	// Clean up JSON TCHAR buffer
	delete[] szJsonTchar;

	// Check if request was successful
	if (!response.success || response.statusCode != 200)
	{
		if (response.pContent != NULL)
		{
			delete[] response.pContent;
		}
		return false;
	}

	// Parse JSON response
	if (response.pContent == NULL)
	{
		return false;
	}

	rapidjson::Document responseDoc;
	responseDoc.Parse(response.pContent);
	
	if (responseDoc.HasParseError() || !responseDoc.IsObject())
	{
		delete[] response.pContent;
		return false;
	}

	// Extract AccessToken from response
	if (!responseDoc.HasMember("AccessToken") || !responseDoc["AccessToken"].IsString())
	{
		delete[] response.pContent;
		return false;
	}

	const char* accessTokenUtf8 = responseDoc["AccessToken"].GetString();
	
	// Convert UTF-8 token to TCHAR
	int tokenTcharLen = MultiByteToWideChar(CP_UTF8, 0, accessTokenUtf8, -1, NULL, 0);
	if (tokenTcharLen <= 0 || (size_t)tokenTcharLen > tokenSize)
	{
		delete[] response.pContent;
		return false;
	}

	MultiByteToWideChar(CP_UTF8, 0, accessTokenUtf8, -1, szTokenOut, (int)tokenSize);

	// Clean up response content
	delete[] response.pContent;

	return true;
}
