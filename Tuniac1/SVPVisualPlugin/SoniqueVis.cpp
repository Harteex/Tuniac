#include "stdafx.h"
#include "SoniqueVis.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Externam vises! Visual

SoniqueVisExternal::SoniqueVisExternal()
{
	p_PluginInfo	= NULL;
	m_DllInst		= NULL;
}

SoniqueVisExternal::~SoniqueVisExternal()
{
}

bool SoniqueVisExternal::LoadFromExternalDLL(LPTSTR PluginName)
{
	m_DllInst		= LoadLibrary ( PluginName );

	if( m_DllInst )
	{
		QueryModule = (QUERYMODULE)GetProcAddress(m_DllInst, "QueryModule");
		if(QueryModule)
		{
			p_PluginInfo = (VisInfo*)QueryModule();
			if(p_PluginInfo)
			{
				char settingsdir[2048];
				StrCat(PluginName, TEXT(".ini"));
				WideCharToMultiByte(CP_ACP, 0, PluginName, 512, settingsdir, 512, NULL, NULL);

				p_PluginInfo->OpenSettings(settingsdir);

				p_PluginInfo->Initialize();

				if (p_PluginInfo->Version > 1) 
					p_PluginInfo->ReceiveQueryInterface(&QInterface);

				return true;
			}
		}
	}

	return false;
};

bool SoniqueVisExternal::Shutdown(void)
{
	DeInit();

	QueryModule		= NULL;
	p_PluginInfo	= NULL;

	if( m_DllInst )
	{
		FreeLibrary( m_DllInst );
		m_DllInst = 0;
	}

	return true;
}


void SoniqueVisExternal::Init()
{
	if( p_PluginInfo )
		p_PluginInfo->Initialize();
}

void SoniqueVisExternal::DeInit()
{
	if( p_PluginInfo )
	{
		if (p_PluginInfo->Version > 0)
		{
			p_PluginInfo->Deinit();
		}
	}
}

char* SoniqueVisExternal::GetName()
{ 
	if( p_PluginInfo )
		return p_PluginInfo->PluginName; 
	else 
		return "SVP renderer";
};

BOOL SoniqueVisExternal::Render( unsigned long *Video, int width, int height, int pitch, VisData* pVD )
{
	if( p_PluginInfo )
	{
		return p_PluginInfo->Render(Video, width, height, pitch, pVD);
	}
	return false;
};

BOOL SoniqueVisExternal::SaveSettings( char* szFileName )
{
	if( p_PluginInfo )
		return p_PluginInfo->SaveSettings(szFileName);

	return false;
};

BOOL SoniqueVisExternal::LoadSettings( char* szFileName )
{
	if( p_PluginInfo )
		return p_PluginInfo->OpenSettings(szFileName);

	return false;
};

void SoniqueVisExternal::Clicked(int x, int y)
{
	if(p_PluginInfo)
	{
		if (p_PluginInfo->Version > 0)
		{
			p_PluginInfo->Clicked(x, y, 1);
		}
	}
};
