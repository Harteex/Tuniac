#include "stdafx.h"
#include "JellyfinService.h"
#include "resource.h"

BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD  ulReason,
	LPVOID lpReserved
)
{
	return TRUE;
}

extern "C" __declspec(dllexport) ITuniacServicePlugin * CreateTuniacPlugin(void)
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
	return PLUGINFLAGS_ABOUT | PLUGINFLAGS_CONFIG;
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

		SendDlgItemMessage(hDlg, IDC_FULLSCREENINHIBIT_CHECK, BM_SETCHECK, /*pPopupNotify->m_bAllowInhibit*/true ? BST_CHECKED : BST_UNCHECKED, 0);

		SendDlgItemMessage(hDlg, IDC_TRIGGER_MANUAL, BM_SETCHECK, /*pPopupNotify->m_bManualTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);
		SendDlgItemMessage(hDlg, IDC_TRIGGER_MANUALBLIND, BM_SETCHECK, /*pPopupNotify->m_bManualBlindTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);
		SendDlgItemMessage(hDlg, IDC_TRIGGER_AUTO, BM_SETCHECK, /*pPopupNotify->m_bAutoTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);
		SendDlgItemMessage(hDlg, IDC_TRIGGER_AUTOBLIND, BM_SETCHECK, /*pPopupNotify->m_bAutoBlindTrigger*/true ? BST_CHECKED : BST_UNCHECKED, 0);


		SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, CB_RESETCONTENT, 0, 0);
		SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, WM_SETTEXT, 0, (LPARAM)/*pPopupNotify->m_szNormalFormatString*/TEXT("fixme"));
		SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@T - @A"));
		SendDlgItemMessage(hDlg, IDC_FORMATTING_NORMALFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@A - @T"));

		SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, CB_RESETCONTENT, 0, 0);
		SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, WM_SETTEXT, 0, (LPARAM)/*pPopupNotify->m_szStreamFormatString*/TEXT("fixme"));
		SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@T - @A"));
		SendDlgItemMessage(hDlg, IDC_FORMATTING_STREAMFORMAT, CB_ADDSTRING, 0, (LPARAM)TEXT("@A - @T"));


	}
	break;

	/*case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_FULLSCREENINHIBIT_CHECK:
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
		break;

		case IDOK:
		case IDCANCEL:
		{
			EndDialog(hDlg, wParam);
			return TRUE;
		}
		break;
		}
		break;*/

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
