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

#include "stdafx.h"
#include ".\audiostream.h"

#include <intrin.h>


#define CopyFloat(dst, src, num) CopyMemory(dst, src, (num) * sizeof(float))

CAudioStream::CAudioStream()
{
	m_bEntryPlayed	= false;

	m_PlayState		= STATE_UNKNOWN;
	m_FadeState		= FADE_NONE;



	fVolume			= 1.0f;
	m_bEntryPlayed	= false;
	m_bMixNotify	= false;
	m_bFinishNotify	= false;
	m_bIsFinished	= false;



	fReplayGainTrack	= 1.0f;
	fReplayGainAlbum	= 1.0f;

	bTrackHasGain	= false;

	bUseAlbumGain	= false;


	m_ulLastSeekMS	= 0;

	m_CrossfadeTimeMS = 0;

	m_Output	= NULL;
}

CAudioStream::~CAudioStream(void)
{

}

bool CAudioStream::Initialize(IAudioSource * pSource, CAudioOutput * pOutput, LPTSTR szSource)
{
	unsigned long srate;
	szURL			= szSource;
	m_pSource		= pSource;
	m_Output		= pOutput;


	if(m_pSource->GetFormat(&srate, &m_Channels))
	{
		if(!pOutput->SetFormat(srate, m_Channels))
			return false;
	}
	else
		return false;

	m_Packetizer.SetPacketSize(m_Output->GetBlockSize());
	m_Output->SetCallback(this);

	m_bServiceThreadRun = true;
	CreateThread(NULL, 0, serviceThreadStub, this, 0, NULL);

	return true;
}

bool CAudioStream::Shutdown(void)
{
	m_bServiceThreadRun = false;
	Stop(); 

	if(m_Output)
	{
		m_Output->Destroy();
		m_Output = NULL;
	}

	if(m_pSource)
	{
		m_pSource->Destroy();
	}

	return true;
}

void CAudioStream::Destroy()
{
	Shutdown();
	delete this;
}

bool			CAudioStream::SetReplayGainScale(float trackscale, float albumscale)
{
	//float fReplayGainScale = pow(10, fReplayGain / 20);

	if(trackscale == 0.0 )
		bTrackHasGain = false;
	else
		bTrackHasGain = true;

	if(albumscale == 0.0 )
		bAlbumHasGain = false;
	else
		bAlbumHasGain = true;
	

	fReplayGainTrack = pow(10, trackscale / 20);;
	fReplayGainAlbum = pow(10, albumscale / 20);;

	return true;
}

void			CAudioStream::EnableReplayGain(bool bEnable)
{
	bReplayGain = bEnable;
}

void			CAudioStream::UseAlbumGain(bool bUse)
{
	bUseAlbumGain = bUse;
}

bool			CAudioStream::SetVolumeScale(float scale)
{
	fVolumeScale = scale/100.0f;
	return true;
}

bool			CAudioStream::SetAmpGain(float scale)
{

	// used when replaygain is enabled, default -6db non replaygain files
	fAmpGain = pow(10, scale / 20.0);

	return true;
}

int			CAudioStream::ServiceStream(void)
{
	if(m_Packetizer.IsFinished())
	{
		return -1;
	}

	CAutoLock t(&m_Lock);
	while(!m_Packetizer.IsBufferAvailable())
	{
		float *			pBuffer			= NULL;
		unsigned long	ulNumSamples	= 0;

		if(m_pSource->GetBuffer(&pBuffer, &ulNumSamples))
		{
			if(ulNumSamples)
			{
				m_Packetizer.WriteData(pBuffer, ulNumSamples);
			}
			else
			{
				return 0;
			}
		}
		else
		{
			// there are no more buffers to get!!
			m_Packetizer.Finished();
			return -1;
		}
	}

	return 0;
}

DWORD CAudioStream::serviceThreadStub(void * pData)
{
	CAudioStream * pStream = (CAudioStream*)pData;

	return pStream->serviceThread();
}

DWORD CAudioStream::serviceThread(void)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	while(m_bServiceThreadRun)
	{
		int ssres = ServiceStream();

		if(ssres==0)
			Sleep(10);

		if(ssres ==-1)
			return 0;
	}

	return 0;
}

int			CAudioStream::GetBuffer(float * pAudioBuffer, unsigned long NumSamples)
{
	CAutoLock t(&m_Lock);

	unsigned long ulSongLength = GetLength();

	if(m_Packetizer.AnyMoreBuffer())
	{

		if(m_Packetizer.GetBuffer(pAudioBuffer))
		{
			// WE CAN ACTUALLY DO 8 SAMPLES AT ONCE
			// THEN 4 + whatevers left
			// + leftover
			//because we only need 3 SSE registers and we have 8 to play with
			// WE NEED TO APPLY VOLUME AND REPLAYGAIN NO MATTER WHAT ANYWAY SO DO THEM HERE!!!
			__m128 XMM0;
			__m128 XMM1 = _mm_load1_ps(&fVolumeScale);
			__m128 XMM2;

			if(bUseAlbumGain && bAlbumHasGain)
			{
				XMM2 = _mm_load1_ps(&fReplayGainAlbum);
			}
			else if(bTrackHasGain && !bUseAlbumGain)
			{
				XMM2 = _mm_load1_ps(&fReplayGainTrack);
			}
			else
			{
				XMM2 = _mm_load1_ps(&fAmpGain);
			}
			
			for(unsigned long x=0; x<NumSamples; x+=4)
			{
				if(NumSamples-x >= 4)
				{
					// load XMM0 with 4 samples from the audio buffer
					XMM0 = _mm_load_ps(((float*)pAudioBuffer)+x);

					// if replaygain enabled in prefs
					if(bReplayGain)
					{
						// apply -6db gain to files without replaygain data
						// or apply whichever value we loaded into XMM2 (track or album)
						XMM0 = _mm_mul_ps(XMM0, XMM2);
					}

					// apply volume
					XMM0 = _mm_mul_ps(XMM0, XMM1);

					// store XMM0 back to where we loaded it from thanks!
					_mm_store_ps(((float*)pAudioBuffer)+x, XMM0);
				}
				else
				{
					int ttt=NumSamples-x;
					for(int ss=0; ss<ttt; ss++)
					{
						float * pSample = ((float*)pAudioBuffer)+x+ss;
						if(bReplayGain)
						{
							if(bUseAlbumGain && bAlbumHasGain)
							{
								*pSample *= fReplayGainAlbum;
							}
							else if(bTrackHasGain && !bUseAlbumGain)
							{
								*pSample *= fReplayGainTrack;
							}
							else
							{
								*pSample *= fAmpGain;
							}
						}

						*pSample *= fVolumeScale;
					}
				}
			}


			for(unsigned long i=0; i<NumSamples; i+=m_Channels)
			{
				for(unsigned long chan=0; chan<m_Channels; chan++)
				{
/* Intrinsic section above replaces this section
					// THIS IS COMPLETELY SSE-ABLE WITH THE CORRECT INTRINSICS
					// TO DO SSEIFY THIS PLSKTHANKX
					// is replaygain set?
					if(bReplayGain)
					{
						// replaygain
						if(bUseAlbumGain && bAlbumHasGain)
						{
							pAudioBuffer[i+chan]		*= fReplayGainAlbum;
						}
						else if(bTrackHasGain && !bUseAlbumGain)
						{
							pAudioBuffer[i+chan]		*= fReplayGainTrack;
						}
						else
						{
							pAudioBuffer[i+chan]		*= fAmpGain;
						}
					}
					// and apply the volume
					pAudioBuffer[i+chan]		*= fVolumeScale;
*/
					// IF WE ARE CROSSFADING WE NEED TO DO THAT TOO!!!
					if(m_FadeState != FADE_NONE)
					{
						// apply the crossfade
						// we REALLY should SSE THIS HERE!!!
						// TODO: SSE THIS PLEASE
						pAudioBuffer[i+chan]		*= fVolume;
					}
				}
				if(m_FadeState != FADE_NONE)
				{
					fVolume += fVolumeChange;
					fVolume = max(0.0f, min(fVolume, 1.0f));
				}
			}


			//check if coreaudio has not been notified yet and if we are crossfading
			if(ulSongLength != LENGTH_UNKNOWN)
			{
				
				unsigned long ulCurrentMS = GetPosition() + m_Output->GetAudioBufferMS();
				if(!m_bEntryPlayed)
				{
					if(ulCurrentMS > (ulSongLength * 0.25))
					{
						m_bEntryPlayed = true;
						tuniacApp.UpdatePlayedCount(szURL);
					}
				}
				if(!m_bMixNotify)
				{
					//length longer then crossfade time
					//

					if((ulSongLength - ulCurrentMS) < m_CrossfadeTimeMS)
					{
						m_bMixNotify = true;
						tuniacApp.CoreAudioMessage(NOTIFY_COREAUDIO_MIXPOINTREACHED, NULL);
					}


				}
			}

			// TODO: if we have played more than 'x' percent of a song send a last played notification back to our controller
			// could be a LastFM thing there!!

			return 1;
		}

		return 0;
	}
	else
	{
		if(m_Packetizer.IsFinished())
		{
			if(m_Output->StreamFinished())
			{
				if(!m_bFinishNotify)
				{
					m_bFinishNotify = true;
					m_bIsFinished = true;
					tuniacApp.CoreAudioMessage(NOTIFY_COREAUDIO_PLAYBACKFINISHED, NULL);
					return -2;
				}
				return -2;
			}

			return -1;
		}
	}

	return 0;
}

unsigned long	CAudioStream::GetState(void)
{
	return m_PlayState;
}

unsigned long	CAudioStream::GetFadeState(void)
{
	return m_FadeState;
}

bool			CAudioStream::IsFinished(void)
{
	return m_bIsFinished;
}

bool			CAudioStream::FadeIn(unsigned long ulMS)
{
	//CAutoLock t(&m_Lock);

	m_FadeState = FADE_FADEIN;
	fVolumeChange =  1.0f / ((float)ulMS * ((float)m_Output->GetSampleRate() / 1000.0f) );
	fVolume = 0.0f;
	return true;
}

bool			CAudioStream::FadeOut(unsigned long ulMS)
{
	//CAutoLock t(&m_Lock);

	m_FadeState = FADE_FADEOUT;
	fVolumeChange =  (-1.0f / ((float)ulMS * ((float)m_Output->GetSampleRate() / 1000.0f)  )   );
	fVolume  = 1.0f;
	return true;
}

bool			CAudioStream::Start(void)
{
	m_PlayState = STATE_PLAYING;
	tuniacApp.CoreAudioMessage(NOTIFY_COREAUDIO_PLAYBACKSTARTED, NULL);
	return m_Output->Start();
}

bool			CAudioStream::Stop(void)
{
	m_PlayState = STATE_STOPPED;
	return m_Output->Stop();
}

unsigned long	CAudioStream::GetLength(void)
{
	unsigned long Length;

	if(m_pSource->GetLength(&Length))
		return Length;

	return LENGTH_UNKNOWN;
}

unsigned long	CAudioStream::GetPosition(void)
{
	unsigned long MSOutput = (m_Output->GetSamplesOut() / (m_Output->GetSampleRate()/1000));

	return(MSOutput + m_ulLastSeekMS);
}

bool			CAudioStream::SetPosition(unsigned long MS)
{
	CAutoLock t(&m_Lock);

	if(m_FadeState != FADE_NONE)
	{
		fVolume		= 1.0;
		m_FadeState = FADE_NONE;
	}

	unsigned long Pos = MS;

	bool bReturn = false;

	m_Output->Stop();
	if(m_pSource->SetPosition(&Pos))
	{
		m_Packetizer.Reset();
		m_Output->Reset();
		m_ulLastSeekMS = Pos;

		bReturn =  true;
	}

	if(m_PlayState == STATE_PLAYING)
		m_Output->Start();

	return bReturn;
}

bool			CAudioStream::GetVisData(float * ToHere, unsigned long ulNumSamples)
{
	if(m_Output == NULL)
		return false;

	return m_Output->GetVisData(ToHere, ulNumSamples);
}
