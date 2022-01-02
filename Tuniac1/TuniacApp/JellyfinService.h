#pragma once

#include "Preferences.h"
#include "TuniacApp.h"
#include <Shlwapi.h>

class CJellyfinService
{
public:
	CJellyfinService(void);
	~CJellyfinService(void);

	bool CanHandle(LPTSTR szSource);
	void TranslateSource(LPTSTR szTranslatedSource, LPTSTR szSource);

protected:

private:

	TCHAR m_szHost[128];
	TCHAR m_szDevice[128];
	TCHAR m_szVersion[128];
	TCHAR m_szToken[128];
};
