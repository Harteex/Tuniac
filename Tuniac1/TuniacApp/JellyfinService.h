#pragma once

#include "Preferences.h"
#include "TuniacApp.h"
#include <Shlwapi.h>
#include <WinInet.h>

class CJellyfinService
{
public:
	CJellyfinService(void);
	~CJellyfinService(void);

	bool CanHandle(LPTSTR szSource);
	bool TranslateSource(LPTSTR szDest, size_t size, LPTSTR szSource);
	bool Login(LPTSTR szHost, LPTSTR szUsername, LPTSTR szPassword);

	void DoHttpRequest();
	void DoHttpRequest2();

	bool ExtractHostnameAndSsl(LPTSTR szHost, LPTSTR szOutHostname, bool* bOutUseSsl);

protected:

private:

	TCHAR m_szHost[256];
	TCHAR m_szDevice[128];
	TCHAR m_szVersion[128];
	TCHAR m_szToken[128];

	TCHAR m_szHostname[256];
	bool m_bUseSsl;
};
