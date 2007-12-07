#include "StdAfx.h"
#include ".\svprenderer.h"
#include "Transform.h"

#include <emmintrin.h>

extern HANDLE	hInst;

/*
					SoniqueVisExternal	*t = new SoniqueVisExternal();

					TCHAR oldFolder[2048];

					GetCurrentDirectory(2048, oldFolder);
					SetCurrentDirectory(szFolder);

					if(t->LoadFromExternalDLL(temp))
					{
						StrCat(temp, TEXT(".ini"));

						char settingsdir[512];

						WideCharToMultiByte(CP_ACP, 0, temp, 512, settingsdir, 512, NULL, NULL);

						t->LoadSettings(settingsdir);
						m_VisArray.AddTail(t);
					}

					delete t;

					SetCurrentDirectory(oldFolder);
*/

__m64 Get_m64(__int64 n)
{
    union __m64__m64
    {
        __m64 m;
        __int64 i;
    } mi;

    mi.i = n;
    return mi.m;
}

#if defined (_M_IX86)

void ChangeBrightnessC_MMX(
    BYTE* pSource, 
    BYTE* pDest, 
    int nNumberOfPixels, 
    int nChange)
{
    if ( nChange > 255 )
        nChange = 255;
    else if ( nChange < -255 )
        nChange = -255;

    BYTE b = (BYTE) abs(nChange);

    // make 64 bits value with b in each byte
    __int64 c = b;

    for ( int i = 1; i <= 7; i++ )
    {
        c = c << 8;
        c |= b;
    }

    // 8 pixels are processed in one loop
    int nNumberOfLoops = nNumberOfPixels / 8;

    __m64* pIn = (__m64*) pSource;          // input pointer
    __m64* pOut = (__m64*) pDest;           // output pointer

    __m64 tmp;                              // work variable


    _mm_empty();                            // emms

    __m64 nChange64 = Get_m64(c);

    if ( nChange > 0 )
    {
        for ( int i = 0; i < nNumberOfLoops; i++ )
        {
            tmp = _m_paddusb(*pIn, nChange64); // Unsigned addition 
                                                 // with saturation.
                                                 // tmp = *pIn + nChange64
                                                 // for each byte

            *pOut = tmp;

            pIn++;                               // next 8 pixels
            pOut++;
        }
    }
    else
    {
        for ( int i = 0; i < nNumberOfLoops; i++ )
        {
            tmp = _mm_subs_pu8(*pIn, nChange64); // Unsigned subtraction 
                                                 // with saturation.
                                                 // tmp = *pIn - nChange64
                                                 // for each byte

            *pOut = tmp;

            pIn++;                                      // next 8 pixels
            pOut++;
        }
    }

    _mm_empty();                            // emms
}

#else

void ChangeBrightnessC_MMX(
    BYTE* pSource, 
    BYTE* pDest, 
    int nNumberOfPixels, 
    int nChange)
{
	for(int rep = 0; rep < nNumberOfPixels; rep++)
	{
		int x = (int)*pSource;

		x-=nChange;

		if(x<0)
			x=0;

		*pDest = (BYTE)x;

		pSource++;
	}
}

#endif

bool SVPRenderer::AddFolderOfSVP(LPTSTR	szFolder)
{
	WIN32_FIND_DATA		w32fd;
	HANDLE				hFind;

	TCHAR	szFilename[1024];

	StrCpyN(szFilename, szFolder, 1024);
	PathAddBackslash(szFilename);
	StrCat(szFilename, TEXT("*.*"));

	hFind = FindFirstFile( szFilename, &w32fd); 
	if(hFind != INVALID_HANDLE_VALUE) 
	{
		do
		{
			if(StrCmp(w32fd.cFileName, TEXT(".")) == 0 || StrCmp(w32fd.cFileName, TEXT("..")) == 0 )
				continue;

			TCHAR temp[1024];

			StrCpy(temp, szFolder);
			PathAddBackslash(temp);
			StrCat(temp, w32fd.cFileName);

			if(GetFileAttributes(temp) & FILE_ATTRIBUTE_DIRECTORY)
			{
				AddFolderOfSVP(temp);	
			}
			else
			{
				LPTSTR ext = PathFindExtension(temp);

				if(!StrCmpI(ext, TEXT(".SVP")))
				{
					LPTSTR string = (LPTSTR)malloc(2048 * sizeof(TCHAR));

					StrCpy(string, temp);

					m_VisFilenameArray.AddTail(string);
				}
			}
		} while(FindNextFile(hFind, &w32fd));

		FindClose(hFind); 
	}

	return true;
}


SVPRenderer::SVPRenderer(void)
{
	m_glDC			= NULL;
	m_glRC			= NULL;

	m_LastWidth			= 0;
	m_LastHeight		= 0;
	m_SelectedVisual	= 0;
	m_TheVisual			= NULL;

	TCHAR				szVisualsPath[2048];

	GetModuleFileName((HMODULE)hInst, szVisualsPath, 512);
	PathRemoveFileSpec(szVisualsPath);
	PathAddBackslash(szVisualsPath);
	StrCat(szVisualsPath, TEXT("svp"));
	PathAddBackslash(szVisualsPath);

	AddFolderOfSVP(szVisualsPath);

	m_hArrow = (HBITMAP)LoadImage((HINSTANCE)hInst, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    ::GetObject (m_hArrow, sizeof (m_ArrowBM), &m_ArrowBM);
}

SVPRenderer::~SVPRenderer(void)
{
}

bool SVPRenderer::RenderVisual(void)
{
	if(m_TheVisual)
	{
		VisData vd;

		unsigned long NumChannels = (unsigned long)m_pHelper->GetVariable(Variable_NumChannels);
		if(NumChannels == -1)
			return true;

		unsigned long temp = 0;

		static float vis[1024*6];
		this->m_pHelper->GetVisData(vis, 512 * NumChannels);

		for(int x=0; x<512; x++)
		{
			vd.Waveform[0][x] = (signed char)(vis[temp]*127.0f);

			if(NumChannels > 1)
			{
				vd.Waveform[1][x] = (signed char)(vis[temp+1]*127.0f);
			}

			temp+=NumChannels;
		}

		if(m_TheVisual->NeedsSpectrum())
		{
			FastFFT_M8(vd.Spectrum[0], (signed char *)vd.Waveform[0]);
			if(NumChannels > 1)
				FastFFT_M8(vd.Spectrum[1], (signed char *)vd.Waveform[1]);
		}
		vd.MillSec	= (unsigned long)m_pHelper->GetVariable(Variable_PositionMS);

		if(m_TheVisual->NeedsVisFX())
			ChangeBrightnessC_MMX(m_textureData, m_textureData, 512*512*4, -8);

		return m_TheVisual->Render(m_textureData, TEXSIZE, TEXSIZE, TEXSIZE, &vd);
	}
	else
		ZeroMemory(m_textureData, TEXSIZE*TEXSIZE*4);

	return true;
}


void	SVPRenderer::Destroy(void)
{
	delete this;
}

LPTSTR	SVPRenderer::GetPluginName(void)
{
	return TEXT("Sonique SVP Visual Plugin");
}

unsigned long SVPRenderer::GetFlags(void)
{
	return PLUGINFLAGS_CONFIG;
}

bool	SVPRenderer::SetHelper(ITuniacVisHelper *pHelper)
{
	m_pHelper = pHelper;
	return true;
}

bool	SVPRenderer::Attach(HDC hDC)
{
	GLuint		PixelFormat;			// Holds The Results After Searching For A Match

	m_LastWidth	 = 0;
	m_LastHeight = 0;

	CalcRepositionTable();

	m_textureData = (GLubyte*)VirtualAlloc(NULL, TEXSIZE*TEXSIZE*4, MEM_COMMIT, PAGE_READWRITE);

	if(m_VisFilenameArray.GetCount())
	{
		SetActiveVisual(m_SelectedVisual);
	}

	PIXELFORMATDESCRIPTOR pfd ;
	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR)) ;
	pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR); 
	pfd.nVersion   = 1 ; 
	pfd.dwFlags    =	PFD_DOUBLEBUFFER |
						PFD_SUPPORT_OPENGL |
						PFD_DRAW_TO_WINDOW |
						PFD_GENERIC_ACCELERATED;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24 ;
	pfd.cDepthBits = 32 ;
	pfd.iLayerType = PFD_MAIN_PLANE ;


	m_glDC = hDC;

	if (!(PixelFormat=ChoosePixelFormat(m_glDC,&pfd)))	// Did Windows Find A Matching Pixel Format?
	{
		return false;								// Return FALSE
	}

	if(!SetPixelFormat(hDC,PixelFormat,&pfd))		// Are We Able To Set The Pixel Format?
	{
		return false;								// Return FALSE
	}

	if (!(m_glRC=wglCreateContext(m_glDC)))				// Are We Able To Get A Rendering Context?
	{
		return false;								// Return FALSE
	}

	if(!wglMakeCurrent(m_glDC, m_glRC))					// Try To Activate The Rendering Context
	{
		return false;								// Return FALSE
	}

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear (GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);			// Enable Blending
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable (GL_TEXTURE_2D); /* enable texture mapping */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,	GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,	GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,		GL_CLAMP  );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,		GL_CLAMP  );

	m_LastMove = GetTickCount();

	return true;
}
bool	SVPRenderer::Detach()
{
	if (m_glRC)												// Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL,NULL))						// Are We Able To Release The DC And RC Contexts?
		{
		}

		if (!wglDeleteContext(m_glRC))						// Are We Able To Delete The RC?
		{
		}

		m_glRC=NULL;										// Set RC To NULL
	}

	VirtualFree(m_textureData, 0, MEM_RELEASE);

	m_glDC = NULL;
	return true;
}

bool	SVPRenderer::Render(int w, int h)
{
	CAutoLock m(&m_RenderLock);

	// just to make sure!
	if(!wglMakeCurrent(m_glDC, m_glRC))					// Try To Activate The Rendering Context
	{
		return false;									// Return FALSE
	}

	if((m_LastWidth != w) || (m_LastHeight != h))
	{
		m_LastWidth		= w;
		m_LastHeight	= h;

		SetRect(&m_NextVisRect, 
				w-(64+16),
				h-(64+16),
				w-(64+16)+64,
				h-(64+16)+64);

		SetRect(&m_PrevVisRect, 
				(16),
				h-(64+16),
				(16)+64,
				h-(64+16)+64);

		glViewport(0,0,w,h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0.0, (float)w, (float)h, 0.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear (GL_COLOR_BUFFER_BIT);
	glLoadIdentity();					// Reset The Modelview Matrix


	this->RenderVisual();


	glTexImage2D(	GL_TEXTURE_2D, 
					0, 
					GL_RGB,
					TEXSIZE, 
					TEXSIZE, 
					0,
					GL_BGRA_EXT, 
					GL_UNSIGNED_BYTE, 
					m_textureData);


	glColor4f(1,1,1,1);
	glEnable(GL_TEXTURE_2D);
	glBegin (GL_QUADS);
		glTexCoord2d(0.0, 0.0);
		glVertex2f( 0.0f,  0.0f);	// Bottom Left Of The Texture and Quad
		glTexCoord2d(0.0, 1.0);
		glVertex2f( 0.0f,  m_LastHeight);	// Bottom Right Of The Texture and Quad
		glTexCoord2d(1.0, 1.0);
		glVertex2f( m_LastWidth, m_LastHeight);	// Top Right Of The Texture and Quad
		glTexCoord2d(1.0, 0.0);
		glVertex2f( m_LastWidth,  0.0f);	// Top Left Of The Texture and Quad
	glEnd ();

	if((GetTickCount() - m_LastMove) < 4000)
	{
		if((GetTickCount() - m_LastMove) < 2000)
		{
			glColor4f(1,1,1,1);
		}
		else
		{
			int val = (GetTickCount() - m_LastMove) - 2000;
			float scale = (float) (2000-val) / 2000.0f;
			glColor4f(1,1,1,scale);
		}

		glEnable(GL_TEXTURE_2D);

		glTexImage2D(	GL_TEXTURE_2D, 
						0, 
						GL_ALPHA,
						m_ArrowBM.bmWidth, 
						m_ArrowBM.bmHeight, 
						0,
						GL_ALPHA, 
						GL_UNSIGNED_BYTE, 
						m_ArrowBM.bmBits);

		glBegin (GL_QUADS);
			glTexCoord2d(1.0, 1.0); glVertex2i(m_NextVisRect.left,	m_NextVisRect.top);
			glTexCoord2d(1.0, 0.0); glVertex2i(m_NextVisRect.left,	m_NextVisRect.bottom);
			glTexCoord2d(0.0, 0.0); glVertex2i(m_NextVisRect.right, m_NextVisRect.bottom);
			glTexCoord2d(0.0, 1.0); glVertex2i(m_NextVisRect.right,	m_NextVisRect.top);
		glEnd();

		glBegin (GL_QUADS);
			glTexCoord2d(0.0, 0.0); glVertex2i(m_PrevVisRect.left,	m_PrevVisRect.top);
			glTexCoord2d(0.0, 1.0); glVertex2i(m_PrevVisRect.left,	m_PrevVisRect.bottom);
			glTexCoord2d(1.0, 1.0); glVertex2i(m_PrevVisRect.right, m_PrevVisRect.bottom);
			glTexCoord2d(1.0, 0.0); glVertex2i(m_PrevVisRect.right,	m_PrevVisRect.top);
		glEnd();
	}

	glFlush();
	SwapBuffers(m_glDC);

	return true;
}

bool	SVPRenderer::About(HWND hWndParent)
{
	return true;
}
bool	SVPRenderer::Configure(HWND hWndParent)
{
	return true;
}

bool	SVPRenderer::Notify(unsigned long Notification)
{
	return true;
}

bool SVPRenderer::SetActiveVisual(int vis)
{
	CAutoLock m(&m_RenderLock);

	TCHAR	szVisFilename[2048];
	char settingsdir[2048];

	if(m_TheVisual)
	{
		StrCpy(szVisFilename, m_VisFilenameArray[m_SelectedVisual]);
		StrCat(szVisFilename, TEXT(".ini"));
		WideCharToMultiByte(CP_ACP, 0, szVisFilename, 512, settingsdir, 512, NULL, NULL);

		SoniqueVisExternal * t = m_TheVisual;
		m_TheVisual = NULL;
		t->SaveSettings(settingsdir);
		t->Shutdown();
		delete t;
	}


	// set no visual
	if(vis == -1)
		return true;

	TCHAR	szPath[2048];
	StrCpy(szPath, m_VisFilenameArray[vis]);
	PathRemoveFileSpec(szPath);
	PathAddBackslash(szPath);

	//TCHAR	szVisFilename[2048];
	StrCpy(szVisFilename, m_VisFilenameArray[vis]);

	TCHAR oldFolder[2048];
	GetCurrentDirectory(2048, oldFolder);
	SetCurrentDirectory(szPath);

	SoniqueVisExternal * newVis = new SoniqueVisExternal();
	if(newVis->LoadFromExternalDLL(m_VisFilenameArray[vis]))
	{
		StrCat(szVisFilename, TEXT(".ini"));
		//char settingsdir[2048];
		WideCharToMultiByte(CP_ACP, 0, szVisFilename, 512, settingsdir, 512, NULL, NULL);

		newVis->LoadSettings(settingsdir);

		m_TheVisual = newVis;
		m_SelectedVisual = vis;
	}
	else
	{
		delete newVis;
	}

	SetCurrentDirectory(oldFolder);


	return true;
}

bool	SVPRenderer::MouseFunction(unsigned long function, int x, int y)
{
	if(function == VISUAL_MOUSEFUNCTION_DOWN)
	{
		POINT pt;

		pt.x = x;
		pt.y = y;

		if(m_VisFilenameArray.GetCount())
		{
			if(m_VisFilenameArray.GetCount() == 1)
				return true;

			m_LastMove = GetTickCount();

			if(m_TheVisual==NULL)
			{
				SetActiveVisual(0);
				return true;
			}

			if(PtInRect(&m_NextVisRect, pt))
			{
				if(m_SelectedVisual < m_VisFilenameArray.GetCount()-1)
				{
					SetActiveVisual(m_SelectedVisual+1);
				}
				else
				{
					SetActiveVisual(0);
				}


				return true;
			}
			else if(PtInRect(&m_PrevVisRect, pt))
			{
				if(m_SelectedVisual > 0)
				{
					SetActiveVisual(m_SelectedVisual-1);
				}
				else
				{
					SetActiveVisual(m_VisFilenameArray.GetCount()-1);
				}

				return true;
			}


			// we need to scale the point to a 512x512 point then send it to the vis

			int visx = (int)(((float)x / (float)m_LastWidth) * 512.0);
			int visy = (int)(((float)y / (float)m_LastHeight) * 512.0);

			if(m_TheVisual)
				m_TheVisual->Clicked(visx,visy);
		}
	}

	return true;
}
