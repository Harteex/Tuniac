/*
	Copyright (C) 2003-2008 Tony Million

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgment in the product documentation is required.

	2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.

	3. This notice may not be removed or altered from any source distribution.
*/
/*
	Modification and addition to Tuniac originally written by Tony Million
	Copyright (C) 2003-2014 Brett Hoyle
*/

#pragma once

#include <setjmp.h>
#include "jpeglib.h"
#include "png.h"
//#include "spng.h"

class CAlbumArt
{
protected:
	CCriticalSection		m_ArtLock;

	LPVOID					m_pBitmapData;
	unsigned long			m_ulBitmapWidth;
	unsigned long			m_ulBitmapHeight;

	struct jpeg_decompress_struct	cinfo;
	struct jpeg_error_mgr			jerr;

	char					szErrorMessage[JMSG_LENGTH_MAX];
	TCHAR					szCurrentArtSource[MAX_PATH];

public:
	CAlbumArt(void);
	~CAlbumArt(void);

	LPTSTR	GetCurrentArtSource(void);
	void	SetCurrentArtSource(LPTSTR szNewArtSource);

	bool	LoadSource(LPVOID pCompressedData, unsigned long ulDataLength, LPTSTR szMimeType);
	bool	SetSource(LPTSTR szFilename);


	static void	errorExit(j_common_ptr cinfo);
	static jmp_buf	sSetjmpBuffer;

	void	SetJPEGErrorMessage(char * szError);


	bool	Draw(HDC hDC, long x, long y, long lWidth, long lHeight);
};
