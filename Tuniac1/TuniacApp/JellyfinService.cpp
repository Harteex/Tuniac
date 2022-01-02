#include "stdafx.h"
#include "JellyfinService.h"

#define CLIENT_NAME TEXT("Tuniac")

CJellyfinService::CJellyfinService(void)
{
	StringCchCopy(m_szHost, 128, TEXT("")); // FIXME should be read from preferences! Hardcode for now...

	StringCchCopy(m_szDevice, 128, TEXT("Windows"));

	TCHAR  infoBuf[512];
	DWORD  bufCharCount = 512;
	if (GetComputerName(infoBuf, &bufCharCount))
		StringCchCopy(m_szDevice, 128, infoBuf);

	StringCchCopy(m_szVersion, 128, TEXT("1.0"));
	StringCchCopy(m_szToken, 128, TEXT("")); // FIXME should be read from preferences! Hardcode for now...
}

CJellyfinService::~CJellyfinService(void)
{
}

bool CJellyfinService::CanHandle(LPTSTR szSource)
{
	// TODO Verify length?
	return StrCmpNI(szSource, TEXT("jellyfin://"), 11) == 0;
}

void CJellyfinService::TranslateSource(LPTSTR szTranslatedSource, LPTSTR szSource)
{
	TCHAR itemId[33];
	StringCchCopy(itemId, 33, szSource + 11);
	StringCchPrintf(szTranslatedSource, 512, TEXT("%s/Audio/%s/stream?static=true\r\nX-Emby-Authorization: MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\", Token=\"%s\"\r\n"), m_szHost, itemId, CLIENT_NAME, m_szDevice, m_szDevice, m_szVersion, m_szToken);
}
