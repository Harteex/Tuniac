// TuniacApp.cpp: implementation of the CTuniacApp class.
//m_hInstance
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <strsafe.h>

#include "TuniacApp.h"
#include "resource.h"

#include "VisualWindow.h"
#include ".\tuniacapp.h"

#define szClassName			TEXT("TUNIACWINDOWCLASS")

#define WINDOWUPDATETIMER			1

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTuniacApp::CTuniacApp()
{
}

CTuniacApp::~CTuniacApp()
{
}

bool CTuniacApp::Initialize(HINSTANCE hInstance, LPTSTR szCommandLine)
{
	m_hInstance = hInstance;

#ifndef DEBUG
	m_hOneInstanceOnlyMutex = CreateMutex(NULL, FALSE, szClassName);
	if(GetLastError() == ERROR_ALREADY_EXISTS) 
	{ 
		unsigned long counttry = 0;
		HWND hOtherWnd = FindWindow(szClassName, NULL);
		while(!hOtherWnd)
		{
			Sleep(100);
			hOtherWnd = FindWindow(szClassName, NULL);
			if(!hOtherWnd)
			{
			}
			if(counttry++>100)
				return(0);
		}

		if(lstrlen(szCommandLine))
		{
			COPYDATASTRUCT cds;

			cds.dwData = 0;
			cds.cbData = (lstrlen(szCommandLine) + 1) * sizeof(TCHAR);
			cds.lpData = szCommandLine;

			SendMessage(hOtherWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
		}

		CloseHandle(m_hOneInstanceOnlyMutex);
		return(0);
	}
#endif

	CoInitialize(NULL);
	InitCommonControls();

	m_hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_MAINWINDOWACCEL));

	m_bSavePrefs = true;
	m_bSaveML = true;

	m_Preferences.LoadPreferences();
	m_Skin.Initialize();

	m_SoftPause.bNow = false;
	m_SoftPause.ulAt = INVALID_PLAYLIST_INDEX;
	
	if(!m_CoreAudio.Startup())
		return false;

	if (!m_History.Initialize())
		return false;

	if(!m_MediaLibrary.Initialize())
		return false;

	if(!m_PlaylistManager.Initialize())
		return false;

	if(!m_PluginManager.Initialize())
		return false;

	if (!m_SysEvents.Initialize())
		return false;

	m_Preferences.m_FileAssoc.UpdateExtensionList();

	IWindow * t;
		
	m_SourceSelectorWindow = new CSourceSelectorWindow;
	t = m_SourceSelectorWindow;
	m_WindowArray.AddTail(t);

	t = new CVisualWindow;
	m_WindowArray.AddTail(t);

	m_LogWindow = new CLogWindow();
	t = m_LogWindow;
	m_WindowArray.AddTail(t);

	m_wc.cbSize			= sizeof(WNDCLASSEX); 
	m_wc.style			= 0;
	m_wc.lpfnWndProc	= (WNDPROC)WndProcStub;
	m_wc.cbClsExtra		= 0;
	m_wc.cbWndExtra		= 0;
	m_wc.hInstance		= hInstance;
	m_wc.hIcon			= tuniacApp.m_Skin.GetIcon(THEMEICON_WINDOW);
	m_wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	m_wc.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1);
	m_wc.hIconSm		= tuniacApp.m_Skin.GetIcon(THEMEICON_WINDOW_SMALL);
	m_wc.lpszMenuName	= MAKEINTRESOURCE(IDR_TUNIAC_MENU);
	m_wc.lpszClassName	= szClassName;
	if(!RegisterClassEx(&m_wc))
		return(false);

	RECT r;
	CopyRect(&r, m_Preferences.GetMainWindowRect());

	if(r.top < 0)
		r.top	= 0;
	if(r.left < 0)
		r.left	= 0;
	if(r.right < 530)
		r.right = 530;
	if(r.bottom < 400)
		r.bottom = 400;

	m_hWnd = CreateWindowEx(WS_EX_ACCEPTFILES | WS_EX_APPWINDOW,
							szClassName, 
							TEXT("Tuniac"), 
							WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
							r.left, 
							r.top, 
							r.right, 
							r.bottom, 
							NULL,
							NULL, 
							hInstance,
							this);


	if(!m_hWnd)
	{
		return(false);
	}

	//if(m_Preferences.GetMainWindowMaximized())
		//SendMessage(m_hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);

	CheckMenuItem(GetSubMenu(m_hPopupMenu, 1), ID_EDIT_ALWAYSONTOP, MF_BYCOMMAND | (m_Preferences.GetAlwaysOnTop() ? MF_CHECKED : MF_UNCHECKED));
	if(m_Preferences.GetAlwaysOnTop())
		SetWindowPos(getMainWindow(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	TCHAR szPrefPageTitle[64];
	for(unsigned int iPrefPage = 0; iPrefPage < m_Preferences.GetPreferencesPageCount(); iPrefPage++)
	{
		if(!m_Preferences.GetPreferencesPageName(iPrefPage, szPrefPageTitle, 64))
			break;
        AppendMenu(GetSubMenu(GetSubMenu(m_hPopupMenu, 1), 6), MF_ENABLED, PREFERENCESMENU_BASE + iPrefPage, szPrefPageTitle);
	}

	PostMessage(getMainWindow(), WM_APP, NOTIFY_PLAYLISTSCHANGED, 0);
	RegisterHotkeys();

	COPYDATASTRUCT cds;

	cds.dwData = 0;
	cds.cbData = (lstrlen(szCommandLine) + 1) * sizeof(TCHAR);
	cds.lpData = szCommandLine;

	SendMessage(m_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
	return true;
}

bool CTuniacApp::Shutdown()
{
	m_SysEvents.Shutdown();
	m_PluginManager.Shutdown();

    while(m_WindowArray.GetCount())
	{
		m_WindowArray[0]->Destroy();
		m_WindowArray.RemoveAt(0);
	}

	m_CoreAudio.Shutdown();

	TCHAR				szURL[MAX_PATH];
	if ( SUCCEEDED( SHGetFolderPath( NULL, CSIDL_APPDATA, NULL, 0, szURL ) ) )
	{
		PathAppend( szURL, TEXT("\\Tuniac") );

		CreateDirectory( szURL, 0);
	}

	m_PlaylistManager.Shutdown(m_bSaveML);
	m_MediaLibrary.Shutdown(m_bSaveML);

	m_Taskbar.Shutdown();

	if(m_bSavePrefs)
		m_Preferences.SavePreferences();

	CoUninitialize();
	return true;
}

bool CTuniacApp::Run()
{
	MSG	msg;

	while(GetMessage(&msg, NULL, 0 , 0))
	{
		if(!TranslateAccelerator(m_hWnd, m_hAccel, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return true;
}

LRESULT CALLBACK CTuniacApp::WndProcStub(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CTuniacApp * pTA;

	if(message == WM_NCCREATE)
	{
		LPCREATESTRUCT lpCS = (LPCREATESTRUCT)lParam;
		pTA = (CTuniacApp *)lpCS->lpCreateParams;

		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pTA);
	}
	else
	{
		pTA = (CTuniacApp *)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}

	return(pTA->WndProc(hWnd, message, wParam, lParam));
}

LRESULT CALLBACK CTuniacApp::WndParentProcStub(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return(DefWindowProc(hWnd, message, wParam, lParam));
}

LRESULT CALLBACK CTuniacApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{

		case WM_TIMER:
			{
				if(wParam == SYSEVENTS_TIMERID)
					m_SysEvents.CheckSystemState();

				if(wParam == WINDOWUPDATETIMER)
				{
					TCHAR szWinTitle[512];
					IPlaylist * pPlaylist = m_PlaylistManager.GetActivePlaylist();
					IPlaylistEntry * pIPE = pPlaylist->GetActiveItem();
					
					if(pIPE && m_CoreAudio.GetState() != STATE_UNKNOWN)
					{
						FormatSongInfo(szWinTitle, 512, pIPE, m_Preferences.GetWindowFormatString(), true);
					}
					else
					{
						wnsprintf(szWinTitle, 512, TEXT("Tuniac"));
					}

					SetWindowText(getMainWindow(), szWinTitle);
					m_Taskbar.SetTitle(szWinTitle);
					
					m_PlayControls.UpdateState();
					//RebuildFutureMenu();
				}
			}
			break;

		case WM_CREATE:
			{
				m_hWndStatus =  CreateWindowEx(	0,
												STATUSCLASSNAME,
												TEXT(""),
												WS_CHILD | WS_VISIBLE,
												0, 
												0,
												0,
												0,
												hWnd,
												NULL,
												m_hInstance,
												NULL);
				m_PlayControls.Create(hWnd);

				HDC hDC = GetDC(hWnd);

				m_TinyFont =  CreateFont(	-MulDiv(8, GetDeviceCaps(hDC, LOGPIXELSY), 72),
											0,
											0,
											0,
											0,
											0,
											0,
											0,
											DEFAULT_CHARSET,
											OUT_TT_PRECIS,
											CLIP_DEFAULT_PRECIS,
											PROOF_QUALITY,
											DEFAULT_PITCH | FF_DONTCARE, 
											TEXT("Trebuchet MS"));

				m_SmallFont =  CreateFont(	-MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72),
											0,
											0,
											0,
											0,
											0,
											0,
											0,
											DEFAULT_CHARSET,
											OUT_TT_PRECIS,
											CLIP_DEFAULT_PRECIS,
											PROOF_QUALITY,
											DEFAULT_PITCH | FF_DONTCARE, 
											TEXT("Trebuchet MS"));

				m_SmallMediumFont =  CreateFont(	-MulDiv(10, GetDeviceCaps(hDC, LOGPIXELSY), 72),
													0,
													0,
													0,
													0,
													0,
													0,
													0,
													DEFAULT_CHARSET,
													OUT_TT_PRECIS,
													CLIP_DEFAULT_PRECIS,
													PROOF_QUALITY,
													DEFAULT_PITCH | FF_DONTCARE, 
													TEXT("Trebuchet MS"));


				m_MediumFont =  CreateFont(	-MulDiv(13, GetDeviceCaps(hDC, LOGPIXELSY), 72),
											0,
											0,
											0,
											0,
											0,
											0,
											0,
											DEFAULT_CHARSET,
											OUT_TT_PRECIS,
											CLIP_DEFAULT_PRECIS,
											PROOF_QUALITY,
											DEFAULT_PITCH | FF_DONTCARE, 
											TEXT("Trebuchet MS"));

				m_LargeFont =  CreateFont(	-MulDiv(15, GetDeviceCaps(hDC, LOGPIXELSY), 72),
											0,
											0,
											0,
											0,
											0,
											0,
											0,
											DEFAULT_CHARSET,
											OUT_TT_PRECIS,
											CLIP_DEFAULT_PRECIS,
											PROOF_QUALITY,
											DEFAULT_PITCH | FF_DONTCARE, 
											TEXT("Trebuchet MS"));

				ReleaseDC(hWnd, hDC);

				m_TrayMenu = GetSubMenu(LoadMenu(tuniacApp.getMainInstance(), MAKEINTRESOURCE(IDR_TRAYMENU)), 0);

				if(m_Taskbar.Initialize(hWnd, WM_TRAYICON))
				{
				}

				for(unsigned long x=0; x<m_WindowArray.GetCount(); x++)
				{
					if(!m_WindowArray[x]->CreatePluginWindow(hWnd, m_hInstance))
					{
						MessageBox(hWnd, TEXT("Error Creating Plugin Window."), TEXT("Non Fatal Error..."), MB_OK | MB_ICONSTOP);

						m_WindowArray[x]->Destroy();
						m_WindowArray.RemoveAt(x);
						x--;
					}
				}

				m_WindowArray[0]->Show();

				m_hPopupMenu = GetMenu(hWnd);

				HMENU tMenu = GetSubMenu(m_hPopupMenu, 3);
				while(GetMenuItemCount(tMenu))
				{
					DeleteMenu(tMenu, 0, MF_BYPOSITION);
				}

				for(unsigned long item = 0; item < m_WindowArray.GetCount(); item++)
				{
					AppendMenu(tMenu, MF_ENABLED, MENU_BASE+item, m_WindowArray[item]->GetName());
				}
				m_SourceSelectorWindow->ShowPlaylistAtIndex(m_PlaylistManager.GetActivePlaylistIndex());

				m_hFutureMenu = CreatePopupMenu();

				SetTimer(hWnd, WINDOWUPDATETIMER, 500, NULL);

				ShowWindow(hWnd, SW_SHOW);

				if(m_Preferences.GetTrayIconMode() == TrayIconMinimize)
				{
					m_Taskbar.Show();
					m_Taskbar.SetTitle(TEXT("Tuniac"));
				}

				tuniacApp.m_LogWindow->LogMessage(TEXT("Tuniac"), TEXT("Initialization Complete"));

			}
			break;

		case WM_DESTROY:
			{
				KillTimer(hWnd, WINDOWUPDATETIMER);

				WINDOWPLACEMENT wp;
				wp.length = sizeof(WINDOWPLACEMENT);
				GetWindowPlacement(hWnd, &wp);
				
				RECT r = wp.rcNormalPosition;
				//GetWindowRect(hWnd, &r);

				SetRect(&r, r.left, r.top, r.right - r.left, r.bottom - r.top);
				m_Preferences.SetMainWindowRect(&r);

				for(unsigned long x=0; x<m_WindowArray.GetCount(); x++)
				{
					m_WindowArray[x]->DestroyPluginWindow();
				}

				DestroyWindow(m_hWndStatus);
				DestroyMenu(m_hFutureMenu);

				PostQuitMessage(0);
			}
			break;

		case WM_CLOSE:
			{
				if(m_Preferences.GetMinimizeOnClose() && !(GetKeyState(VK_CONTROL) & 0x8000))
				{
					ShowWindow(hWnd, SW_MINIMIZE);
				}
				else
					DestroyWindow(hWnd);
			}
			break;

		case WM_GETMINMAXINFO:
			{
				LPMINMAXINFO lpMinMaxInfo = (LPMINMAXINFO)lParam;

				lpMinMaxInfo->ptMinTrackSize.x = 530;
				lpMinMaxInfo->ptMinTrackSize.y = 400;
			}
			break;

/*
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				RECT		r;

				GetClientRect(hWnd, &r);

				HDC		hDC = BeginPaint(hWnd, &ps);

				HTHEME hTheme   = OpenThemeData(hWnd, L"HEADER");
				if(hTheme)
				{
					r.top = 95;
					r.bottom = 99;

					DrawThemeBackground(hTheme, hDC, HP_HEADERITEMLEFT, HILS_NORMAL, &r, NULL);
					CloseThemeData(hTheme);
					r.top = 98;
					r.bottom = 99;
					FillRect(hDC, &r, GetSysColorBrush(COLOR_3DSHADOW));

					r.top = 99;
					r.bottom = 100;
					FillRect(hDC, &r, GetSysColorBrush(COLOR_3DDKSHADOW));
				}
				else
				{
					r.top = 98;
					r.bottom = 99;
					FillRect(hDC, &r, GetSysColorBrush(COLOR_3DSHADOW));

					r.top = 99;
					r.bottom = 100;
					FillRect(hDC, &r, GetSysColorBrush(COLOR_3DDKSHADOW));
				}

				EndPaint(hWnd, &ps);
			}
			break;
*/

		case WM_SIZE:
			{
				WORD Width = LOWORD(lParam);
				WORD Height = HIWORD(lParam);
//				RECT clientRect;
				RECT playcontrolsrect = {0,0, Width, 60};
				RECT statusRect;

				if(wParam == SIZE_MINIMIZED)
				{
					if(m_Preferences.GetTrayIconMode() == TrayIconMinimize)
					{
						ShowWindow(hWnd, SW_HIDE);
						m_Taskbar.Show();
						break;
					}
				}

				m_PlayControls.Move(playcontrolsrect.left, playcontrolsrect.top, playcontrolsrect.right, playcontrolsrect.bottom);

				SendMessage(m_hWndStatus, message, wParam, lParam);
				SendMessage(m_hWndStatus, SB_GETRECT, 0, (LPARAM)&statusRect);

				for(unsigned long x=0; x < m_WindowArray.GetCount(); x++)
				{
					m_WindowArray[x]->SetPos(	0,
												playcontrolsrect.bottom,
												Width,
												Height - statusRect.bottom - playcontrolsrect.bottom);
				}
			}
			break;

		case WM_DROPFILES:
			{
				HDROP hDrop = (HDROP)wParam;

				TCHAR szURL[1024];

				UINT uNumFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, NULL);

				if(m_MediaLibrary.BeginAdd(uNumFiles))
				{
					for(UINT file = 0; file<uNumFiles; file++)
					{
						DragQueryFile(hDrop, file, szURL, 2048);
						m_MediaLibrary.AddItem(szURL);
					}

					m_MediaLibrary.EndAdd();
				}

				DragFinish(hDrop);

			}
			break;

		case WM_MENUSELECT:
			{
				CheckMenuItem(GetSubMenu(m_hPopupMenu, 2), ID_PLAYBACK_SHUFFLEPLAY, m_Preferences.GetRandomState() ? MF_CHECKED | MF_BYCOMMAND : MF_UNCHECKED | MF_BYCOMMAND);

				CheckMenuItem(GetSubMenu(m_hPopupMenu, 2), ID_PLAYBACK_SOFTPAUSE, m_SoftPause.bNow ? MF_CHECKED | MF_BYCOMMAND : MF_UNCHECKED | MF_BYCOMMAND);

				CheckMenuRadioItem(GetSubMenu(GetSubMenu(m_hPopupMenu, 2), 7), 0, 3, m_Preferences.GetRepeatMode(), MF_BYPOSITION);
				EnableMenuItem(GetSubMenu(GetSubMenu(m_hPopupMenu, 2), 7), RepeatAllQueued, MF_BYPOSITION | (m_MediaLibrary.m_Queue.GetCount() == 0 ? MF_GRAYED : MF_ENABLED));

				EnableMenuItem(GetSubMenu(m_hPopupMenu, 2), ID_PLAYBACK_CLEARQUEUE, m_MediaLibrary.m_Queue.GetCount() == 0 ? MF_GRAYED : MF_ENABLED);
				EnableMenuItem(GetSubMenu(m_hPopupMenu, 2), ID_PLAYBACK_CLEARPAUSEAT, m_SoftPause.ulAt == INVALID_PLAYLIST_INDEX ? MF_GRAYED : MF_ENABLED);
			}
			break;

		case WM_APPCOMMAND:
			{
				DWORD cmd  = GET_APPCOMMAND_LPARAM(lParam);
				//DWORD uDevice = GET_DEVICE_LPARAM(lParam);
				//DWORD dwKeys = GET_KEYSTATE_LPARAM(lParam);

				switch(cmd)
				{
					case APPCOMMAND_MEDIA_PLAY_PAUSE:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PLAYPAUSE, 0), 0);
							return(TRUE);
						}
						break;

					case APPCOMMAND_MEDIA_STOP:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_STOP, 0), 0);
							return(TRUE);
						}
						break;

					case APPCOMMAND_MEDIA_PAUSE:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PAUSE, 0), 0);
							return(TRUE);
						}
						break;

					case APPCOMMAND_MEDIA_PLAY:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PLAY, 0), 0);
							return(TRUE);
						}
						break;

					case APPCOMMAND_MEDIA_NEXTTRACK:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_NEXT, 0), 0);
							return(TRUE);
						}
						break;

					case APPCOMMAND_MEDIA_PREVIOUSTRACK:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PREVIOUS, 0), 0);
							return(TRUE);
						}
						break;
					case APPCOMMAND_MEDIA_FAST_FORWARD:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_SEEKFORWARD, 0), 0);
							return(TRUE);
						}
						break;
					case APPCOMMAND_MEDIA_REWIND:
						{
							SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_SEEKBACK, 0), 0);
							return(TRUE);
						}
						break;
				}
			}
			break;

		case WM_APP:
			{
				switch (wParam)
				{
					case NOTIFY_MIXPOINTREACHED:
						{
							IPlaylist * pPlaylist = m_PlaylistManager.GetActivePlaylist();

							/*// handle infinite streams differently
							CMediaLibraryPlaylistEntry * pCurrent = m_MediaLibrary.GetItemByID(pPlaylist->GetActiveItem()->GetEntryID());
							if(pCurrent != NULL && (unsigned long)pCurrent->GetField(FIELD_KIND) == ENTRY_KIND_URL && (unsigned long)pCurrent->GetField(FIELD_PLAYBACKTIME) == (-1))
							{
								TCHAR szOldURL[512];
								StrCpyN(szOldURL, pCurrent->GetLibraryEntry()->szURL[0], 512);
								int i = 1;
								do
								{
									if(wcslen(pCurrent->GetLibraryEntry()->szURL[i]) < 1)
										break;
									StrCpyN(pCurrent->GetLibraryEntry()->szURL[i - 1], pCurrent->GetLibraryEntry()->szURL[i], 512);
									i++;
								} while(i < LIBENTRY_MAX_URLS);
								StrCpyN(pCurrent->GetLibraryEntry()->szURL[i - 1], szOldURL, 512);
								m_CoreAudio.SetSource((IPlaylistEntry *)pCurrent);
								m_SourceSelectorWindow->UpdateView();
								break;
							}*/

							bool bOK = true;
							bool bFADE = true;

							if(tuniacApp.m_Preferences.GetCrossfadeTime() == 0)
							{
								if(m_Preferences.GetRepeatMode() == RepeatOne)
								{
									m_CoreAudio.Reset();
								}

								if(m_Preferences.GetRepeatMode() == RepeatNone)
								{
									if(!pPlaylist->CheckNext())
									{
										m_CoreAudio.Reset();
									}
								}

								bFADE = false;
							}

					      	if(m_Preferences.GetRepeatMode() != RepeatOne)
							{
								if(m_PlaySelected.GetCount() > 0)
								{
									if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
									{
										IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;
										
										pPlaylistEX->SetActiveIndex(m_PlaySelected[0]);
										m_PlaySelected.RemoveAt(0);
										m_SourceSelectorWindow->m_PlaylistSourceView->DeselectFirstItem();
									}
									else
									{
										bOK = pPlaylist->Next();
									}
								}
								else if (m_MediaLibrary.m_Queue.GetCount() > 0)
								{
									IPlaylistEntry * pIPE = m_MediaLibrary.m_Queue.RemoveHead();
									if(m_PlaylistManager.SetActiveByEntry(pIPE))
									{
										bOK = true;
										pPlaylist = m_PlaylistManager.GetActivePlaylist();
									}
									else
									{
                                        bOK = pPlaylist->Next();
									}
								}
								else
								{
									bOK = pPlaylist->Next();
								}
							}

							if(!bOK)
							{
								if(m_Preferences.GetRepeatMode() == RepeatAll)
								{
									if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
									{
										IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;
										pPlaylistEX->SetActiveIndex(INVALID_PLAYLIST_INDEX);
										pPlaylistEX->SetActiveIndex(0);
										bOK = true;
									}
								}
							}

							if(bOK)
							{
								IPlaylistEntry * pIPE = pPlaylist->GetActiveItem();
								if(bFADE)
								{
									if(m_CoreAudio.TransitionTo(pIPE))
									{
										m_CoreAudio.Play();
									}

									if(m_SoftPause.bNow || m_SoftPause.ulAt == pIPE->GetEntryID())
									{
										m_SoftPause.bNow = false;
										m_SoftPause.ulAt = INVALID_PLAYLIST_INDEX;
										SendMessage(tuniacApp.getMainWindow(), WM_MENUSELECT, 0, 0);
										m_CoreAudio.Reset();
									}
									else
									{
										m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE, NULL, NULL);
									}

								}
								else
								{
									if(m_CoreAudio.SetSource(pIPE))
									{
										if (m_SoftPause.bNow || m_SoftPause.ulAt == pIPE->GetEntryID())
										{
											m_SoftPause.bNow = false;
											m_SoftPause.ulAt = INVALID_PLAYLIST_INDEX;
											SendMessage(tuniacApp.getMainWindow(), WM_MENUSELECT, 0, 0);
										}
										else
										{
											m_CoreAudio.Play();
											m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE, NULL, NULL);
										}
									}
								}

								m_SourceSelectorWindow->UpdateView();

								if(m_SourceSelectorWindow->GetVisiblePlaylistIndex() == m_PlaylistManager.GetActivePlaylistIndex())
									if(m_Preferences.GetFollowCurrentSongMode())
										m_SourceSelectorWindow->ShowCurrentlyPlaying();

							}
						}
						break;

					case NOTIFY_PLAYBACKFINISHED:
						{
							if(m_Preferences.GetRepeatMode() == RepeatNone)
							{
								m_CoreAudio.Reset();
							}
						}
						break;

					case NOTIFY_PLAYBACKSTARTED:
						{
							m_SourceSelectorWindow->UpdateView();
							IPlaylistEntry * pIPE = m_PlaylistManager.GetActivePlaylist()->GetActiveItem();
							if (pIPE)
								m_History.AddItem(pIPE);
						}
						break;

					case NOTIFY_PLAYLISTSCHANGED:
						{
//							m_Taskbar.UpdatePlaylistMenu();
						}
						break;
				}
			}
			break;

		case WM_HOTKEY:
			{
				if(wParam == m_aPlay)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PLAYPAUSE, 0), 0);
				}
				if(wParam == m_aStop)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_STOP, 0), 0);
				}
				else if(wParam == m_aNext)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_NEXT, 0), 0);
				}
				else if(wParam == m_aRandNext)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_RANDOMNEXT, 0), 0);
				}
				else if(wParam == m_aPrev)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PREVIOUS, 0), 0);
				}
				else if(wParam == m_aPrevByHist)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PREVIOUS_BYHISTORY, 0), 0);
				}
				else if(wParam == m_aShuffle)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_TOGGLE_SHUFFLE, 0), 0);
				}
				else if(wParam == m_aRepeat)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_TOGGLE_REPEAT, 0), 0);
				}
				else if(wParam == m_aVolUp)
				{
					m_CoreAudio.SetVolumeScale((m_CoreAudio.GetVolumePercent() * 0.01) + 0.1f);
					m_PlayControls.UpdateVolume();
				}
				else if(wParam == m_aVolDn)
				{
					m_CoreAudio.SetVolumeScale((m_CoreAudio.GetVolumePercent() * 0.01) - 0.1f);
					m_PlayControls.UpdateVolume();
				}
				else if(wParam == m_aSeekForward)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_SEEKFORWARD, 0), 0);
				}
				else if(wParam == m_aSeekBack)
				{
					SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_SEEKBACK, 0), 0);
				}

			}
			break;

		case WM_COPYDATA:
			{
				PCOPYDATASTRUCT pCDS = (PCOPYDATASTRUCT)lParam;

				switch(pCDS->dwData)
				{
					case 0:
						{ 
							// command line from previous (or current) version
							bool		bAddingFiles = false, bPlayAddedFiles = false, bQueueAddedFiles = false;
							bool		bExitAtEnd = false, bWantFocus = false, bForceNoFocus = false;
							unsigned long ulMLOldCount = m_MediaLibrary.GetCount();
							LPWSTR		*szArglist;
							int			nArgs;
							int			i;
							szArglist	= CommandLineToArgvW((LPTSTR)pCDS->lpData, &nArgs);

							if(szArglist != NULL)
							{
								for(i = 1; i < nArgs; i++)
								{

									if(PathFileExists(szArglist[i]) || PathIsURL(szArglist[i]))
									{
										if(!bAddingFiles)
										{
											if(!m_MediaLibrary.BeginAdd(nArgs - 1)) break;
											bAddingFiles = true;
											bWantFocus = true;
										}

										m_MediaLibrary.AddItem(szArglist[i]);
									}
									else if(StrCmpI(szArglist[i], TEXT("-queue")) == 0)
										bQueueAddedFiles = true;

									else if(StrCmpI(szArglist[i], TEXT("-play")) == 0)
										bPlayAddedFiles = true;

									else if(StrCmpI(szArglist[i], TEXT("-pause")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PAUSE, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-togglepause")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PLAYPAUSE, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-stop")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_STOP, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-softpause")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_SOFTPAUSE, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-next")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_NEXT, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-randomnext")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_RANDOMNEXT, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-prev")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PREVIOUS, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-toggleshuffle")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_TOGGLE_SHUFFLE, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-togglerepeat")) == 0)
										SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_TOGGLE_REPEAT, 0), 0); 

									else if(StrCmpI(szArglist[i], TEXT("-exit")) == 0)
									{
										bExitAtEnd = true;
										break;
									}

									else if(StrCmpI(szArglist[i], TEXT("-restore")) == 0)
									{
										//TODO: Implement This
//										m_Taskbar.ShowTrayIcon(FALSE);
										bWantFocus = true;
										ShowWindow(hWnd, SW_SHOWNORMAL);
									}

									else if(StrCmpI(szArglist[i], TEXT("-minimize")) == 0)
									{
										//TODO: Implement This
										//										m_Taskbar.ShowTrayIcon(TRUE);
										ShowWindow(hWnd, SW_MINIMIZE);
									}
									else if(StrCmpI(szArglist[i], TEXT("-nofocus")) == 0)
										bForceNoFocus = true;

									else if(StrCmpI(szArglist[i], TEXT("-dontsaveprefs")) == 0)
										m_bSavePrefs = false;

									else if(StrCmpI(szArglist[i], TEXT("-wipeprefs")) == 0)
									{
										m_Preferences.CleanPreferences();
										m_Preferences.DefaultPreferences();
									}

									else if(StrCmpI(szArglist[i], TEXT("-wipefileassoc")) == 0)
										m_Preferences.m_FileAssoc.CleanAssociations();

									else if(StrCmpI(szArglist[i], TEXT("-dontsaveml")) == 0)
										m_bSaveML = false;

								}

								if(bAddingFiles) m_MediaLibrary.EndAdd();
								if(bPlayAddedFiles)
								{
									if(m_MediaLibrary.GetCount() > ulMLOldCount)
									{
										m_CoreAudio.Reset();
										m_SourceSelectorWindow->ShowPlaylistAtIndex(0);
										m_PlaylistManager.SetActivePlaylist(0);
										if (m_PlaylistManager.GetActivePlaylist()->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
										{
											IPlaylistEX * pPlaylist = (IPlaylistEX *)m_PlaylistManager.GetActivePlaylist();
											m_SourceSelectorWindow->m_PlaylistSourceView->ClearTextFilter();
											pPlaylist->SetActiveIndex(ulMLOldCount);
											m_CoreAudio.SetSource(pPlaylist->GetActiveItem());
										}

									}
									SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_PLAYBACK_PLAY, 0), 0);
								}
								else if (bQueueAddedFiles)
								{
									if(m_MediaLibrary.GetCount() > ulMLOldCount)
									{
										m_PlaylistManager.SetActivePlaylist(0);
										for(int i = ulMLOldCount; i < m_MediaLibrary.GetCount(); i++)
										{
											m_MediaLibrary.m_Queue.Append((IPlaylistEntry *)m_MediaLibrary.GetItemByIndex(i));
										}
									}
								}

								if(bWantFocus && !bForceNoFocus)
									SetForegroundWindow(hWnd);

								if(bExitAtEnd)
									SendMessage(hWnd, WM_COMMAND, MAKELONG(ID_FILE_EXIT, 0), 0);


							}
							LocalFree(szArglist);

						}
						break;
				}
			}
			break;

		case WM_COMMAND:
			{
				WORD wCmdID = LOWORD(wParam);

				if(wCmdID >= MENU_BASE && wCmdID <= (MENU_BASE + m_WindowArray.GetCount()))
				{
					unsigned long m_ActiveScreen = wCmdID - MENU_BASE;

					for(unsigned long item = 0; item < m_WindowArray.GetCount(); item++)
					{
						if(item == m_ActiveScreen)
							m_WindowArray[item]->Show();
						else
							m_WindowArray[item]->Hide();
					}

					return 0;
				}

				if(wCmdID >= PLAYLISTMENU_BASE && wCmdID <= (PLAYLISTMENU_BASE + m_PlaylistManager.GetNumPlaylists()))
				{
					tuniacApp.m_CoreAudio.Reset();
					m_SourceSelectorWindow->ShowPlaylistAtIndex(wCmdID - PLAYLISTMENU_BASE);
					m_PlaylistManager.SetActivePlaylist(wCmdID - PLAYLISTMENU_BASE);
					IPlaylist * pPlaylist = m_PlaylistManager.GetActivePlaylist();

					if(pPlaylist == NULL)
						return 0;

					if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
					{
						((IPlaylistEX *)pPlaylist)->SetActiveIndex(0);
					}
					SendMessage(getMainWindow(), WM_COMMAND, MAKELONG(ID_PLAYBACK_PLAY, 0), 0);
					return 0;
				}

				if(wCmdID >= HISTORYMENU_BASE && wCmdID <= (HISTORYMENU_BASE + m_Preferences.GetHistoryListSize()))
				{
					m_History.PlayHistoryItem(wCmdID - HISTORYMENU_BASE);
					return 0;
				}

				if(wCmdID >= FUTUREMENU_BASE && wCmdID <= (FUTUREMENU_BASE + m_Preferences.GetFutureListSize()))
				{
					int iFromCurrent = wCmdID - FUTUREMENU_BASE + 1;
					IPlaylistEntry * pEntry = GetFuturePlaylistEntry(iFromCurrent);

					if(pEntry && m_CoreAudio.SetSource(pEntry))
					{
						m_CoreAudio.Play();
						m_PlaylistManager.SetActiveByEntry(pEntry);
						m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE_MANUAL, NULL, NULL);
					}						
				}


				switch(wCmdID)
				{

					case ID_APP_SHOW:
						{
							ShowWindow(hWnd, SW_SHOWNORMAL);
							SetForegroundWindow(hWnd);
						}
						break;

					case ID_FILE_ADDFILES:
						{
#define OFNBUFFERSIZE		(32*1024)
							OPENFILENAME		ofn;
							TCHAR				szURLBuffer[OFNBUFFERSIZE];

							ZeroMemory(&ofn, sizeof(OPENFILENAME));
							StringCbCopy(szURLBuffer, OFNBUFFERSIZE, TEXT(""));

							ofn.lStructSize			= sizeof(OPENFILENAME);
							ofn.hwndOwner			= hWnd;
							ofn.hInstance			= m_hInstance;
							ofn.lpstrFilter			= TEXT("All Files\0*.*\0");
							ofn.lpstrCustomFilter	= NULL;
							ofn.nMaxCustFilter		= 0;
							ofn.nFilterIndex		= 0;
							ofn.lpstrFile			= szURLBuffer;
							ofn.nMaxFile			= OFNBUFFERSIZE;
							ofn.lpstrFileTitle		= NULL;
							ofn.nMaxFileTitle		= 0;
							ofn.lpstrInitialDir		= NULL;
							ofn.lpstrTitle			= NULL;
							ofn.Flags				= OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

							if(GetOpenFileName(&ofn))
							{

								if(m_MediaLibrary.BeginAdd(BEGIN_ADD_UNKNOWNNUMBER))
								{
									if(ofn.nFileOffset < lstrlen(szURLBuffer))
									{
										m_MediaLibrary.AddItem(szURLBuffer);
									}
									else
									{
										TCHAR szFilePath[256];
										TCHAR szURL[256];

										LPTSTR	szOFNName = &szURLBuffer[ofn.nFileOffset];

										StringCbCopy( szFilePath, 256, szURLBuffer );
										while( lstrlen(szOFNName) != 0 )
										{
											StringCbCopy( szURL, 256, szFilePath );
											StringCbCat( szURL, 256, TEXT("\\") );
											StringCbCopy( szURL, 256, szOFNName );

											m_MediaLibrary.AddItem(szURL);

											szOFNName = &szOFNName[lstrlen(szOFNName) + 1];
										}
									}

									m_MediaLibrary.EndAdd();
								}
							}
						}
						break;

					case ID_FILE_ADDDIRECTORY:
						{
							LPMALLOC lpMalloc;  // pointer to IMalloc

							if(::SHGetMalloc(&lpMalloc) == NOERROR)
							{
								TCHAR szBuffer[1024];

								BROWSEINFO browseInfo;
								LPITEMIDLIST lpItemIDList;

								browseInfo.hwndOwner		= hWnd;
								browseInfo.pidlRoot			= NULL; 
								browseInfo.pszDisplayName	= NULL;
								browseInfo.lpszTitle		= TEXT("Select a directory...");   // passed in
								browseInfo.ulFlags			= BIF_RETURNONLYFSDIRS | BIF_USENEWUI;   // also passed in
								browseInfo.lpfn				= NULL;      // not used
								browseInfo.lParam			= 0;      // not used   

								if((lpItemIDList = ::SHBrowseForFolder(&browseInfo)) != NULL)
								{
									if(::SHGetPathFromIDList(lpItemIDList, szBuffer))
									{
										if(m_MediaLibrary.BeginAdd(BEGIN_ADD_UNKNOWNNUMBER))
										{
											m_MediaLibrary.AddItem(szBuffer);
											m_MediaLibrary.EndAdd();
										}
									}

									lpMalloc->Free(lpItemIDList);
								}

								lpMalloc->Release();

							}
						}
						break;

					case ID_FILE_ADDOTHER:
						{
							TCHAR szAddBuffer[2048] = TEXT("");
							if(DialogBoxParam(m_hInstance, MAKEINTRESOURCE(IDD_ADDOTHER), hWnd, (DLGPROC)AddOtherProc, (LPARAM)szAddBuffer))
							{
								if(wcslen(szAddBuffer) > 0 && m_MediaLibrary.BeginAdd(BEGIN_ADD_UNKNOWNNUMBER))
								{
									if(!m_MediaLibrary.AddItem(szAddBuffer))
									{
										MessageBeep(-1);
									}
									m_MediaLibrary.EndAdd();
								}
							}
						}
						break;

					case ID_FILE_FORCESAVEMEDIALIBRARY:
						{
							bool bOK;
							bOK = m_MediaLibrary.SaveMediaLibrary();
							bOK = m_PlaylistManager.SavePlaylistLibrary() && bOK;
							
							if(bOK)
							{
								TCHAR szMessage[128];
								wnsprintf(szMessage, 128, TEXT("MediaLibrary has been saved.\n%d entries total.\n%d standard playlists."), 
												m_MediaLibrary.GetCount(), 
												m_PlaylistManager.m_StandardPlaylists.GetCount());

								tuniacApp.m_LogWindow->LogMessage(TEXT("MediaLibrary"), szMessage);
							}
						}
						break;

					case ID_FILE_NEWPLAYLIST_NORMALPLAYLIST:
						{
							m_PlaylistManager.CreateNewStandardPlaylist(TEXT("Untitled Playlist"));
							m_SourceSelectorWindow->UpdateList();
						}
						break;

					case ID_FILE_NEWPLAYLIST_SMARTPLAYLIST:
						{
						}
						break;

					case ID_FILE_EXPORTPLAYLIST:
						{
							if(m_SourceSelectorWindow->GetVisiblePlaylist()->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
							{
								IPlaylistEX * pPlaylist = (IPlaylistEX *)m_SourceSelectorWindow->GetVisiblePlaylist();
								IndexArray indexArray;
								EntryArray exportArray;
								if(m_SourceSelectorWindow->m_PlaylistSourceView->GetSelectedIndexes(indexArray))
								{
									for(int i = 0; i < indexArray.GetCount(); i++)
									{
										IPlaylistEntry * pIPE = pPlaylist->GetItemAtIndex(indexArray[i]);
										if(pIPE)
											exportArray.AddTail(pIPE);
									}
									m_MediaLibrary.m_ImportExport.Export(exportArray, NULL);
								}
								else
								{
									MessageBox(getMainWindow(), TEXT("Select entries in the playlist to export."), TEXT("Export"), MB_OK | MB_ICONINFORMATION);
								}
							}
						}
						break;

					case ID_FILE_EXIT:
						{
							DestroyWindow(hWnd);
						}
						break;

					//Added Mark - 7th October
					case ID_EDIT_EDITFILEINFORMATION:
						{
							m_SourceSelectorWindow->GetVisibleView()->EditTrackInfo();
						}
						break;

					case ID_EDIT_SHOWCURRENTLYPLAYINGTRACK:
						{
							m_SourceSelectorWindow->ShowCurrentlyPlaying();
						}
						break;

					case ID_EDIT_SHOWVIEWOPTIONS:
						{
							m_SourceSelectorWindow->ShowActiveViewViewOptions(hWnd);
						}
						break;
					
					case ID_EDIT_ALWAYSONTOP:
						{
							bool bOnTop = !m_Preferences.GetAlwaysOnTop();
							CheckMenuItem(GetSubMenu(m_hPopupMenu, 1), ID_EDIT_ALWAYSONTOP, MF_BYCOMMAND | (bOnTop ? MF_CHECKED : MF_UNCHECKED));
							m_Preferences.SetAlwaysOnTop(bOnTop);
							SetWindowPos(getMainWindow(), bOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						}
						break;

					case ID_EDIT_PREFERENCES:
						{
							m_Preferences.ShowPreferences(hWnd, 0);
						}
						break;

					case ID_HELP_ABOUT:
						{
							TCHAR szAbout[512];
							wnsprintf(szAbout, 512, TEXT("Tuniac Audio Player (Beta)\nBuild: %s\n\nBased on code originally developed by Tony Million.\nNow developed by: Blair 'Blur' McBride.\n\n\nContributers (in chronological order):\n\n%s."), TEXT(__DATE__), TEXT(TUNIACABOUT_CONTRIBUTERS));
							MessageBox(tuniacApp.getMainWindow(), szAbout, TEXT("About"), MB_OK | MB_DEFBUTTON2 | MB_ICONQUESTION);
						}
						break;

					case ID_HELP_TUNIACHELP:
						{
							TCHAR szHelp[512];
							GetModuleFileName(NULL, szHelp, 512);
							PathRemoveFileSpec(szHelp);
							StringCbCat(szHelp, 512, TEXT("\\Guide\\index.html"));
							if(PathFileExists(szHelp) == FALSE)
								StringCbCopy(szHelp, 512, TEXT("http://www.tuniac.com/guide/"));

							ShellExecute(NULL, NULL, szHelp, NULL, NULL, SW_SHOW);
						}
						break;

					case ID_PLAYBACK_PLAYPAUSE:
						{
							if(m_CoreAudio.GetState() == STATE_PLAYING)
							{
								m_CoreAudio.Stop();
								m_PluginManager.PostMessage(PLUGINNOTIFY_SONGPAUSE, NULL, NULL);
							}
							else
							{
								if(!m_CoreAudio.Play())
								{
									if(tuniacApp.m_PlaylistManager.SetActivePlaylist(m_SourceSelectorWindow->GetVisiblePlaylistIndex()))
									{
										IPlaylist * pPlaylist = tuniacApp.m_PlaylistManager.GetActivePlaylist();

										if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
										{
											IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;

											if(m_Preferences.GetRandomState())
											{
												pPlaylistEX->SetActiveIndex(g_Rand.IRandom(0, pPlaylistEX->GetNumItems()));
											}
											else
											{
												pPlaylistEX->SetActiveIndex(0);
											}
										}


										IPlaylistEntry * pIPE = pPlaylist->GetActiveItem();
										if(pIPE)
										{
											if(m_CoreAudio.SetSource(pIPE))
											{
												m_CoreAudio.Play();
												m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE_MANUAL, NULL, NULL);
											}
										}
										m_SourceSelectorWindow->UpdateView();
									}
								}
							}
							if(m_CoreAudio.GetState() == STATE_PLAYING)
								m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE_MANUAL, NULL, NULL);
						}
						break;

					case ID_PLAYBACK_STOP:
						{
							m_CoreAudio.Stop();
							m_CoreAudio.SetPosition(0);
							m_PluginManager.PostMessage(PLUGINNOTIFY_SONGSTOP, NULL, NULL);
						}
						break;

					case ID_PLAYBACK_PAUSE:
						{
							if(m_CoreAudio.GetState() == STATE_PLAYING)
							{
								m_CoreAudio.Stop();
								m_PluginManager.PostMessage(PLUGINNOTIFY_SONGPAUSE, NULL, NULL);
							}
						}
						break;

					case ID_PLAYBACK_PLAY:
						{
							if(!m_CoreAudio.Play())
							{
								if(tuniacApp.m_PlaylistManager.SetActivePlaylist(m_SourceSelectorWindow->GetVisiblePlaylistIndex()))
								{
									IPlaylist * pPlaylist = tuniacApp.m_PlaylistManager.GetActivePlaylist();

									if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
									{
										IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;

										if(m_Preferences.GetRandomState())
										{
											pPlaylistEX->SetActiveIndex(g_Rand.IRandom(0, pPlaylistEX->GetNumItems()));
										}
										else
										{
											pPlaylistEX->SetActiveIndex(0);
										}
									}

									IPlaylistEntry * pIPE = pPlaylist->GetActiveItem();
									if(pIPE)
									{
										if(m_CoreAudio.SetSource(pIPE))
										{
											m_CoreAudio.Play();
										}
									}
									m_SourceSelectorWindow->UpdateView();
								}
							}
							if(m_CoreAudio.GetState() == STATE_PLAYING)
								m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE_MANUAL, NULL, NULL);

						}
						break;

					case ID_PLAYBACK_SOFTPAUSE:
						{
							m_SoftPause.bNow = !m_SoftPause.bNow;
						}
						break;

					case ID_PLAYBACK_NEXT:
						{
							IPlaylist * pPlaylist = m_PlaylistManager.GetActivePlaylist();
							if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED && m_PlaySelected.GetCount() > 0)
							{
								IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;
								
								if(pPlaylistEX->GetActiveIndex() == m_PlaySelected[0])
								{
									pPlaylistEX->SetActiveIndex(m_PlaySelected[0]);
									m_PlaySelected.RemoveAt(0);
									m_SourceSelectorWindow->m_PlaylistSourceView->DeselectFirstItem();
								}

								pPlaylistEX->SetActiveIndex(m_PlaySelected[0]);
								m_PlaySelected.RemoveAt(0);
								m_SourceSelectorWindow->m_PlaylistSourceView->DeselectFirstItem();

								IPlaylistEntry * pIPE = pPlaylistEX->GetActiveItem();
								unsigned long State = m_CoreAudio.GetState();
								if(m_CoreAudio.SetSource(pIPE))
								{
									if(State == STATE_PLAYING)
										m_CoreAudio.Play();
								}

							}
							else if (m_MediaLibrary.m_Queue.GetCount() > 0)
							{
								IPlaylistEntry * pIPE = m_MediaLibrary.m_Queue.RemoveHead();

								if(m_Preferences.GetRepeatMode() == RepeatAllQueued)
									m_MediaLibrary.m_Queue.Append(pIPE);

								unsigned long State = m_CoreAudio.GetState();
								if(m_CoreAudio.SetSource(pIPE))
								{

									bool bFound = false;
									if(tuniacApp.m_PlaylistManager.GetActivePlaylist()->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
									{
										IPlaylistEX * pPlaylist = (IPlaylistEX *)tuniacApp.m_PlaylistManager.GetActivePlaylist();
										for (unsigned long i = 0; i < pPlaylist->GetNumItems(); i++)
										{
											if(pPlaylist->GetItemAtIndex(i) == pIPE)
											{
												pPlaylist->SetActiveIndex(i);
												bFound = true;
												break;
											}
										}
									}
									if(!bFound)
									{
										tuniacApp.m_PlaylistManager.SetActivePlaylist(0);
										IPlaylistEX * pPlaylist = (IPlaylistEX *)tuniacApp.m_PlaylistManager.GetPlaylistAtIndex(0);
										for (unsigned long i = 0; i < pPlaylist->GetNumItems(); i++)
										{
											if(pPlaylist->GetItemAtIndex(i) == pIPE)
											{
												pPlaylist->SetActiveIndex(i);
												break;
											}
										}
									}

									if(State == STATE_PLAYING)
										m_CoreAudio.Play();
								}
							}
							else if(pPlaylist->Next())
							{

								// get active item and pass it to the playback manager class
								if(m_SourceSelectorWindow->GetVisiblePlaylistIndex() == m_PlaylistManager.GetActivePlaylistIndex())
									if(m_Preferences.GetFollowCurrentSongMode())
										m_SourceSelectorWindow->ShowCurrentlyPlaying();

								IPlaylistEntry * pIPE = pPlaylist->GetActiveItem();
								unsigned long State = m_CoreAudio.GetState();
								if(m_CoreAudio.SetSource(pIPE))
								{
									if(State == STATE_PLAYING)
										m_CoreAudio.Play();
								}
							}
							else
							{
								if(m_Preferences.GetRepeatMode() == RepeatAll)
								{
									if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
									{
										IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;
										pPlaylistEX->SetActiveIndex(0);
										IPlaylistEntry * pIPE = pPlaylistEX->GetActiveItem();
										unsigned long State = m_CoreAudio.GetState();
										if(m_CoreAudio.SetSource(pIPE) && State == STATE_PLAYING)
											m_CoreAudio.Play();
									}
								}
							}
							m_SourceSelectorWindow->UpdateView();
							if(m_CoreAudio.GetState() == STATE_PLAYING)
								m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE_MANUAL, NULL, NULL);

						}
						break;

					case ID_PLAYBACK_RANDOMNEXT:
						{
							bool bWasRandom = m_Preferences.GetRandomState();
							m_Preferences.SetRandomState(!bWasRandom);
							SendMessage(getMainWindow(), WM_COMMAND, MAKELONG(ID_PLAYBACK_NEXT, 0), 0);
							m_Preferences.SetRandomState(bWasRandom);
						}
						break;

					case ID_PLAYBACK_PREVIOUS:
						{
							if(m_CoreAudio.GetPosition() < 3500)
							{
								IPlaylist * pPlaylist = m_PlaylistManager.GetActivePlaylist();

								if(pPlaylist->Previous())
								{
									// get active item and pass it to the playback manager class

									if(m_SourceSelectorWindow->GetVisiblePlaylistIndex() == m_PlaylistManager.GetActivePlaylistIndex())
										if(m_Preferences.GetFollowCurrentSongMode())
										m_SourceSelectorWindow->ShowCurrentlyPlaying();

									IPlaylistEntry * pIPE = pPlaylist->GetActiveItem();
									unsigned long State = m_CoreAudio.GetState();
									if(m_CoreAudio.SetSource(pIPE))
									{
										if(State == STATE_PLAYING)
											m_CoreAudio.Play();
									}
								}
								else
								{
									m_CoreAudio.Stop();
									m_CoreAudio.SetPosition(0);
								}
								m_SourceSelectorWindow->UpdateView();
								if(m_CoreAudio.GetState() == STATE_PLAYING)
									m_PluginManager.PostMessage(PLUGINNOTIFY_SONGCHANGE_MANUAL, NULL, NULL);

							}
							else
							{
								m_CoreAudio.SetPosition(0);
							}						
							if(m_CoreAudio.GetState() == STATE_PLAYING)
								SendMessage(tuniacApp.getMainWindow(), WM_MENUSELECT, 0, 0);
						}
						break;
					case ID_PLAYBACK_PREVIOUS_BYHISTORY:
						{
							if(m_History.GetCount() > 1)
								m_History.PlayHistoryItem(1);
						}
						break;
					case ID_PLAYBACK_SEEKFORWARD:
						{
							unsigned long pos = m_CoreAudio.GetPosition() + 50;
							m_CoreAudio.SetPosition(pos);
							tuniacApp.m_PluginManager.PostMessage(PLUGINNOTIFY_SEEK_MANUAL, NULL, NULL);
						}
						break;
					case ID_PLAYBACK_SEEKBACK:
						{
							unsigned long pos = m_CoreAudio.GetPosition() - 1300;
							m_CoreAudio.SetPosition(pos);
							tuniacApp.m_PluginManager.PostMessage(PLUGINNOTIFY_SEEK_MANUAL, NULL, NULL);
						}
						break;
					case ID_PLAYBACK_TOGGLE_SHUFFLE:
						{
							m_Preferences.SetRandomState(!m_Preferences.GetRandomState());
						}
						break;
					case ID_PLAYBACK_TOGGLE_REPEAT:
						{
							if(m_Preferences.GetRepeatMode() == RepeatNone)
								m_Preferences.SetRepeatMode(RepeatOne);
							else if(m_Preferences.GetRepeatMode() == RepeatOne)
								m_Preferences.SetRepeatMode(RepeatAll);
							else if(m_Preferences.GetRepeatMode() == RepeatAll)
								m_Preferences.SetRepeatMode(RepeatNone);
						}
						break;

					case ID_PLAYBACK_CLEARQUEUE:
						{
							m_MediaLibrary.m_Queue.Clear();
							if(m_Preferences.GetRepeatMode() == RepeatAllQueued)
							{
								m_Preferences.SetRepeatMode(RepeatAll);
								SendMessage(getMainWindow(), WM_MENUSELECT, 0, 0);
							}
						}
						break;
					case ID_PLAYBACK_CLEARPAUSEAT:
						{
							m_SoftPause.ulAt = INVALID_PLAYLIST_INDEX;
						}
						break;
					case ID_PLAYBACK_SHUFFLEPLAY:
						{
							IPlaylist * pPlaylist = m_PlaylistManager.GetActivePlaylist();

							m_Preferences.SetRandomState(!m_Preferences.GetRandomState());

							/* Somehow here we need to tell the playlist to update itself
							if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
							{
								((IPlaylistEX *)pPlaylist)->ApplyFilter();
							}
							*/

							m_SourceSelectorWindow->UpdateView();
						}
						break;
					case ID_REPEAT_OFF:
						{
							m_Preferences.SetRepeatMode(RepeatNone);
							m_SourceSelectorWindow->UpdateView();
						}
						break;
					case ID_REPEAT_ONETRACK:
						{
							m_Preferences.SetRepeatMode(RepeatOne);
							m_SourceSelectorWindow->UpdateView();
						}
						break;
					case ID_REPEAT_ALLTRACKS:
						{
							m_Preferences.SetRepeatMode(RepeatAll);
							m_SourceSelectorWindow->UpdateView();
						}
						break;
					case ID_REPEAT_ALLQUEUED:
						{
							m_Preferences.SetRepeatMode(RepeatAllQueued);
							m_SourceSelectorWindow->UpdateView();
						}
						break;
				}
			}
			break;

		case WM_SYSCOLORCHANGE:
			{
				m_PlayControls.SysColorChange(wParam, lParam);
			}
			break;

		case WM_TRAYICON:
			{
				UINT uID; 
				UINT uMouseMsg; 
 
				uID = (UINT) wParam; 
				uMouseMsg = (UINT) lParam; 

				switch (uMouseMsg)
				{
					case WM_LBUTTONDBLCLK:
						{
							ShowWindow(hWnd, SW_SHOWNORMAL);
							SetForegroundWindow(hWnd);
						}
						break;

					case WM_CONTEXTMENU:
						{
							POINT pt;
							GetCursorPos(&pt);
							SetForegroundWindow(hWnd);
							TrackPopupMenu(m_TrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
							PostMessage(hWnd, WM_NULL, 0, 0);

						}
						break;
				}

			}
			break;

		default:
			return(DefWindowProc(hWnd, message, wParam, lParam));
			break;
	}

	return(0);
}

bool CTuniacApp::SetStatusText(LPTSTR szStatusText)
{
	SendMessage(m_hWndStatus, SB_SETTEXT, 0, (LPARAM)szStatusText);
	return true;
}

LRESULT CALLBACK CTuniacApp::AddOtherProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static LPTSTR szString = (LPTSTR)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch(message)
	{
		case WM_INITDIALOG:
			{
				SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
				szString = (LPTSTR)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			}
			break;

		case WM_COMMAND:
			{
				WORD wCmdID = LOWORD(wParam);

				switch(wCmdID)
				{
					case IDC_ADDOTHER_ADD:
						{
							GetDlgItemText(hWnd, IDC_ADDOTHER_EDIT, szString, 2048);
							EndDialog(hWnd, 1);
						}
						break;

					case IDC_ADDOTHER_CANCEL:
						{
							EndDialog(hWnd, 0);
						}
						break;
				}
			}
			break;

		default:
			return FALSE;
			break;
	}

	return TRUE;
}

bool CTuniacApp::CoreAudioMessage(unsigned long Message, void * Params)
{
	PostMessage(getMainWindow(), WM_APP, Message, LPARAM(Params));
	return true;
}

bool CTuniacApp::GetShuffleState(void)
{
	return m_Preferences.GetRandomState();
}

bool CTuniacApp::RegisterHotkeys(void)
{
	m_aPlay			 = GlobalAddAtom(TEXT("PLAY"));
	m_aStop			 = GlobalAddAtom(TEXT("STOP"));
	m_aNext			 = GlobalAddAtom(TEXT("NEXT"));
	m_aRandNext		 = GlobalAddAtom(TEXT("RANDNEXT"));
	m_aPrev			 = GlobalAddAtom(TEXT("PREV"));
	m_aPrevByHist	 = GlobalAddAtom(TEXT("PREVBYHISTORY"));
	m_aVolUp		 = GlobalAddAtom(TEXT("VOLUP"));
	m_aVolDn		 = GlobalAddAtom(TEXT("VOLDN"));
	m_aSeekForward	 = GlobalAddAtom(TEXT("SEEKFW"));
	m_aSeekBack		 = GlobalAddAtom(TEXT("SEEKBK"));
	m_aShuffle		 = GlobalAddAtom(TEXT("SHUFTOG"));
	m_aRepeat		 = GlobalAddAtom(TEXT("REPETOG"));

	if(!RegisterHotKey(m_hWnd, m_aPlay,		MOD_WIN, VK_NUMPAD5))
		tuniacApp.m_LogWindow->LogMessage(TEXT("HotKey Register"), TEXT("Error registering hotkey"));
	RegisterHotKey(m_hWnd, m_aStop,			MOD_WIN, VK_NUMPAD0);
	RegisterHotKey(m_hWnd, m_aNext,			MOD_WIN, VK_NUMPAD6);
	RegisterHotKey(m_hWnd, m_aRandNext,		MOD_WIN, VK_NUMPAD9);
	RegisterHotKey(m_hWnd, m_aPrev,			MOD_WIN, VK_NUMPAD4);
	RegisterHotKey(m_hWnd, m_aPrevByHist,	MOD_WIN | MOD_CONTROL, VK_NUMPAD4);

	RegisterHotKey(m_hWnd, m_aVolUp,		MOD_WIN, VK_NUMPAD8);
	RegisterHotKey(m_hWnd, m_aVolDn,		MOD_WIN, VK_NUMPAD2);

	RegisterHotKey(m_hWnd, m_aSeekBack,		MOD_WIN, VK_NUMPAD1);
	RegisterHotKey(m_hWnd, m_aSeekForward,	MOD_WIN, VK_NUMPAD3);

	RegisterHotKey(m_hWnd, m_aShuffle,		MOD_WIN, 'S');
	RegisterHotKey(m_hWnd, m_aRepeat,		MOD_WIN, 'R');
	return true;
}

HFONT	CTuniacApp::GetTuniacFont(int size)
{
	switch(size)
	{
		case FONT_SIZE_LARGE:
			{
				return m_LargeFont;
			}
			break;

		case FONT_SIZE_MEDIUM:
			{
				return m_MediumFont;
			}
			break;
		case FONT_SIZE_SMALL_MEDIUM:
			{
				return m_SmallMediumFont;
			}
			break;
		case FONT_SIZE_SMALL:
			{
				return m_SmallFont;
			}
			break;
		case FONT_SIZE_TINY:
			{
				return m_TinyFont;
			}
			break;
	}

	return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

IPlaylistEntry *	CTuniacApp::GetFuturePlaylistEntry(unsigned long iFromCurrent)
{
	if(iFromCurrent < 0)
	{
		int iBack = 0 - iFromCurrent;
		return tuniacApp.m_History.GetHistoryItem(iBack - 1);
	}

	IPlaylist * pPlaylist = tuniacApp.m_PlaylistManager.GetActivePlaylist();
	if(pPlaylist == NULL)
		return NULL;

	IPlaylistEntry * pEntry = pPlaylist->GetActiveItem();
	if(pEntry == NULL)
		return NULL;

	if(iFromCurrent == 0 || m_Preferences.GetRepeatMode() == RepeatOne)
		return pEntry;

	if(iFromCurrent > 0)
	{

		if(m_PlaySelected.GetCount() >= iFromCurrent && pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
		{
			IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;
			pEntry = pPlaylistEX->GetItemAtIndex(m_PlaySelected[iFromCurrent - 1]);
		}
		else if(m_MediaLibrary.m_Queue.GetCount() >= (iFromCurrent -= m_PlaySelected.GetCount()))
		{
			pEntry = m_MediaLibrary.m_Queue.GetItemAtIndex(iFromCurrent - 1);
		}
		else if(m_Preferences.GetRepeatMode() == RepeatAllQueued && m_MediaLibrary.m_Queue.GetCount() > 0)
		{
			pEntry = m_MediaLibrary.m_Queue.GetItemAtIndex((iFromCurrent % (m_MediaLibrary.m_Queue.GetCount() + 1)));
		}
		else if(pPlaylist->GetFlags() & PLAYLIST_FLAGS_EXTENDED)
		{
			iFromCurrent -= m_MediaLibrary.m_Queue.GetCount();
			IPlaylistEX * pPlaylistEX = (IPlaylistEX *)pPlaylist;
			
			//getactiveindex fails if filtered out due to INVALID_PLAYLIST_INDEX
			int iIndex = pPlaylistEX->GetActiveIndex();
			if(iIndex == INVALID_PLAYLIST_INDEX)
			{
				if(pPlaylistEX->GetNumItems() > 0)
				{
					iIndex = 0;
					iFromCurrent--;
				}
			}
			bool bWrapped = false;

			for(int i = 0; i < iFromCurrent && iIndex != INVALID_PLAYLIST_INDEX; i++)
			{
				iIndex = pPlaylistEX->GetNextIndex(iIndex, &bWrapped);
				if(iIndex == INVALID_PLAYLIST_INDEX)
					return NULL;
				if(bWrapped && m_Preferences.GetRepeatMode() != RepeatAll)
					return NULL;
			}
			if(iIndex == INVALID_PLAYLIST_INDEX)
				return NULL;

			pEntry = pPlaylistEX->GetItemAtIndex(iIndex);
		}
		else
		{
			return NULL;
		}

	}
	return pEntry;
}

bool	CTuniacApp::FormatSongInfo(LPTSTR szDest, unsigned int iDestSize, IPlaylistEntry * pIPE, LPTSTR szFormat, bool bPlayState)
{
	memset(szDest, '\0', iDestSize);
	if(pIPE == NULL)
		return false;

	LPTSTR a = szFormat, b = StrStr(a, TEXT("@"));
	while (b != NULL) {
		if(wcslen(szDest) >= iDestSize - 1)
			break;

		StringCchCopyN(szDest + wcslen(szDest), iDestSize - wcslen(szDest), a, wcslen(a) - wcslen(b));

		int lField = 0xFFFE;
		bool bShort = false;
		WCHAR cFmt = b[1];
		switch(cFmt)
		{
			case '\0':
				lField = 0xFFFF;
				break;
			case '@':
				lField = 0xFFFD;
				break;
			case '!':
				{
					if(bPlayState)
						lField = -1;
					else
						lField = 0xFFFE;
				}
				break;
			case 'U':
				lField = FIELD_URL;
				break;
			case 'F':
				lField = FIELD_FILENAME;
				break;
			case 'X':
				lField = FIELD_FILEEXTENSION;
				break;
			case 'x':
				lField = FIELD_FILEEXTENSION;
				bShort = true;
				break;
			case 'A':
				lField = FIELD_ARTIST;
				break;
			case 'L':
				lField = FIELD_ALBUM;
				break;
			case 'T':
				lField = FIELD_TITLE;
				break;
			case '#':
				lField = FIELD_TRACKNUM;
				break;
			case 'G':
				lField = FIELD_GENRE;
				break;
			case 'Y':
				lField = FIELD_YEAR;
				break;
			case 'I':
				lField = FIELD_PLAYBACKTIME;
				break;
			case 'i':
				lField = FIELD_PLAYBACKTIME;
				bShort = true;
				break;
			case 'K':
				lField = FIELD_KIND;
				break;
			case 'S':
				lField = FIELD_FILESIZE;
				break;
			case 'D':
				lField = FIELD_DATEADDED;
				break;
			case 'E':
				lField = FIELD_DATEFILECREATION;
				break;
			case 'P':
				lField = FIELD_DATELASTPLAYED;
				break;
			case 'Z':
				lField = FIELD_PLAYCOUNT;
				break;
			case 'R':
				lField = FIELD_RATING;
				break;
			case 'C':
				lField = FIELD_COMMENT;
				break;
			case 'B':
				lField = FIELD_BITRATE;
				break;
			case 'M':
				lField = FIELD_SAMPLERATE;
				break;
			case 'N':
				lField = FIELD_NUMCHANNELS;
				break;
		}

		if (lField == 0xFFFF)
		{
			wnsprintf(szDest + wcslen(szDest), iDestSize - wcslen(szDest), TEXT("@"));
			break;
		}
		else if (lField == 0xFFFE)
		{
			wnsprintf(szDest + wcslen(szDest), iDestSize - wcslen(szDest), TEXT("@"));
			b += 1;
		}
		else if (lField == 0xFFFD) 
		{
			wnsprintf(szDest + wcslen(szDest), iDestSize - wcslen(szDest), TEXT("@"));
			b += 2;
		}
		else if (lField == -1)
		{
			TCHAR szPlayState[16];
			switch(m_CoreAudio.GetState())
			{
				case STATE_STOPPED:
					{
						wnsprintf(szPlayState, 16, TEXT("Paused"));
					}
					break;

				case STATE_PLAYING:
					{
						wnsprintf(szPlayState, 16, TEXT("Playing"));
					}
					break;

				default:
					{
						wnsprintf(szPlayState, 16, TEXT("Stopped"));
					}
					break;
			}
			wnsprintf(szDest + wcslen(szDest), iDestSize - wcslen(szDest), szPlayState);
			b += 2;
		}
		else
		{
			TCHAR szFieldData[256];
			pIPE->GetTextRepresentation(lField, szFieldData, 256);

			if(bShort)
			{
				switch(lField)
				{
					case FIELD_PLAYBACKTIME:
						{
							int i = 0;
							while(szFieldData[i] == L'0' && szFieldData[i+1] == L'0')
							{
								i += 3;
								if(i >= wcslen(szFieldData) - 1)
								{
									i = wcslen(szFieldData) - 2;
									break;
								}
							}
							if(i > 0)
							{
								if(szFieldData[i] == L'0')
									i++;
								StringCbCopy(szFieldData, 256, szFieldData + i);
							}
						}
						break;

					case FIELD_FILEEXTENSION:
						{
							if(wcslen(szFieldData) > 0 && szFieldData[0] == L'.')
								StringCbCopy(szFieldData, 256, szFieldData + 1);
						}
						break;
				}
			}

			wnsprintf(szDest + wcslen(szDest), iDestSize - wcslen(szDest), TEXT("%s"), szFieldData);
			b += 2;
		}

		a = b;
		b = StrStr(a, TEXT("@"));
	}

	StringCchCopyN(szDest + wcslen(szDest), iDestSize - wcslen(szDest), a, wcslen(a));
	return true;
}

bool	CTuniacApp::EscapeMenuItemString(LPTSTR szSource, LPTSTR szDest,  unsigned int iDestSize)
{
	memset(szDest, L'\0', iDestSize);
	LPTSTR pszRight, pszLeft = szSource;
	while((pszRight = StrChr(pszLeft, L'&')) != NULL)
	{
		StringCbCatN(szDest, 256, pszLeft, min(iDestSize, (wcslen(pszLeft) + 1 - wcslen(pszRight))*2));
		//StrCatN(szDest, pszLeft, min(iDestSize, wcslen(pszLeft) + 1 - wcslen(pszRight)));
		if(wcslen(szDest) >= iDestSize - 3)
			break;
		StringCbCatN(szDest, 256, TEXT("&&"), 4);
		//StrCatN(szDest, TEXT("&&"), iDestSize);
		pszLeft = pszRight + 1;
	}
	StringCbCatN(szDest, 256, pszLeft, iDestSize*2);
	//StrCatN(szDest, pszLeft, iDestSize);
	szDest[iDestSize - 1] = L'\0';
	return true;
}


void	CTuniacApp::RebuildFutureMenu(void)
{
	while(GetMenuItemCount(m_hFutureMenu) > 0)
		DeleteMenu(m_hFutureMenu, 0, MF_BYPOSITION);

	TCHAR szDetail[112];
	TCHAR szItem[128];
	TCHAR szTime[16];
	for(int i = 0; i < tuniacApp.m_Preferences.GetFutureListSize(); i++)
	{
		tuniacApp.m_PluginManager.GetTrackInfo(szDetail, 112, m_Preferences.GetListFormatString(), i + 1);
		if(wcslen(szDetail) == 0) break;
		EscapeMenuItemString(szDetail, szItem, 112);
		tuniacApp.m_PluginManager.GetTrackInfo(szTime, 16, TEXT("\t[@I]"), i + 1);
		StringCbCatN(szItem, 128, szTime, 128);
		AppendMenu(m_hFutureMenu, MF_STRING, FUTUREMENU_BASE + i, szItem);
	}
}

HMENU	CTuniacApp::GetFutureMenu(void)
{
	return m_hFutureMenu;
}

