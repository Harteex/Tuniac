#include "StdAfx.h"
#include ".\audiooutput.h"

#define CopyFloat(dst, src, num) CopyMemory(dst, src, (num) * sizeof(float))

#define BUFFERSIZEMS			(200)
#define MAX_BUFFER_COUNT		4


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif



///////////////////////////////////////////////////////////////
//					FUNDAMENTAL
//
//
//
//					m_BlockSize is in SAMPLES
//					m_BlockSizeBytes is in bytes calculated from the size of a block * size of a sample!!!!
//					SO FUCKING REMEMBER THIS YOU IDIOTS

CAudioOutput::CAudioOutput(void) :
	m_hThread(NULL),
	m_hEvent(NULL),
	m_pXAudio(NULL),
	m_pSourceVoice(NULL),
	m_pMasteringVoice(NULL),
	m_pfAudioBuffer(NULL),
	m_pCallback(NULL),
	m_BufferInProgress(0)
{
	ZeroMemory(&m_waveFormatPCMEx, sizeof(m_waveFormatPCMEx));
	m_pfAudioBuffer = NULL;
	OutputDebugString(TEXT("CAudioOutput Created\r\n"));
}

CAudioOutput::~CAudioOutput(void)
{
	Shutdown();
	OutputDebugString(TEXT("CAudioOutput Destroyed\r\n"));
}

///////////////////////////////////////////////////////////////////////
//
//


unsigned long CAudioOutput::ThreadStub(void * in)
{
	CAudioOutput * pAO = (CAudioOutput *)in;
	return(pAO->ThreadProc());
}

unsigned long CAudioOutput::ThreadProc(void)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	DWORD currentDiskReadBuffer = 0;
	while(m_bThreadRun)
	{

		float * pOffset = &m_pfAudioBuffer[(currentDiskReadBuffer*m_BlockSize)];

		if(m_pCallback && m_bPlaying)
		{
			if(m_pCallback->GetBuffer(pOffset, m_BlockSize))
			{
				// HACK: this is highly inefficient and must be fixed!
				if(currentDiskReadBuffer == 0)
				{
					CopyFloat(	&m_pfAudioBuffer[(MAX_BUFFER_COUNT*m_BlockSize)], 
								m_pfAudioBuffer,
								m_BlockSize);
				}

				XAUDIO2_BUFFER buf	=	{0};
				buf.Flags			=	0;
				buf.AudioBytes		=	m_BlockSizeBytes;
				buf.pAudioData		=	(BYTE*)pOffset;
				buf.pContext		=	(VOID*)currentDiskReadBuffer;

				HRESULT hr = m_pSourceVoice->SubmitSourceBuffer( &buf );
				if(FAILED(hr))
				{
					MessageBox(NULL, TEXT("Error Writing Buffer"), TEXT("UH OH"), MB_OK);
				}

				currentDiskReadBuffer++;
				currentDiskReadBuffer %= MAX_BUFFER_COUNT;
			}
			else
			{
				// write an empty end of stream buffer!
				XAUDIO2_BUFFER buf	= {0};
				buf.AudioBytes		= 0;
				buf.pAudioData		= (BYTE*)pOffset;
				buf.Flags = XAUDIO2_END_OF_STREAM;
				m_pSourceVoice->SubmitSourceBuffer( &buf );
			}
		}

        //
        // Now that the event has been signaled, we know we have audio available. The next
        // question is whether our XAudio2 source voice has played enough data for us to give
        // it another buffer full of audio. We'd like to keep no more than MAX_BUFFER_COUNT - 1
        // buffers on the queue, so that one buffer is always free for disk I/O.
        //
        XAUDIO2_VOICE_STATE state;
        while( (m_pSourceVoice->GetState( &state ), state.BuffersQueued >= MAX_BUFFER_COUNT) && m_bThreadRun)
        {
			WaitForSingleObject( m_hEvent, m_Interval );
        }
	}

	return(0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Initialization and termination
//
//
//

bool CAudioOutput::Initialize(void)
{
	if(FAILED(XAudio2Create(&m_pXAudio)))
	{
		//booo we dont have an XAudio2 object!
		m_pXAudio = NULL;
		return false;
	}

	m_pXAudio->StartEngine();

	HRESULT hr;

    if ( FAILED(hr = m_pXAudio->CreateMasteringVoice( &m_pMasteringVoice ) ) )
    {
        wprintf( L"Failed creating mastering voice: %#X\n", hr );
        return false;
    }

	HRESULT chr = m_pXAudio->CreateSourceVoice(	&m_pSourceVoice, 
										(const WAVEFORMATEX*)&m_waveFormatPCMEx, 
										0, 
										1.0f, 
										this);
	if(chr != S_OK)
	{
		return false;
	}

	//m_pfAudioBuffer = (float*)malloc((m_BlockSizeBytes) * (MAX_BUFFER_COUNT+1));
		
	m_pfAudioBuffer = (float *)VirtualAlloc(NULL, 
											(m_BlockSizeBytes) * (MAX_BUFFER_COUNT+1), 
											MEM_COMMIT, 
											PAGE_READWRITE);		// allocate audio memory
	VirtualLock(m_pfAudioBuffer, 
				(m_BlockSizeBytes) * (MAX_BUFFER_COUNT+1));


	m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	m_bThreadRun = true;
	m_hThread = CreateThread(	NULL,
								16384,
								ThreadStub,
								this,
								0,
								&m_dwThreadID);

	if(!m_hThread)
		return false;


	return(true);
}

bool CAudioOutput::Shutdown(void)
{
	if(m_hThread)
	{
		m_bThreadRun = false;

		SetEvent(m_hEvent);

		if(WaitForSingleObject(m_hThread, 10000) == WAIT_TIMEOUT)
			TerminateThread(m_hThread, 0);

		CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	if(m_hEvent)
	{
		CloseHandle(m_hEvent);
		m_hEvent = NULL;
	}

	if(m_pfAudioBuffer)
	{
//		free(m_pfAudioBuffer);
		VirtualUnlock(	m_pfAudioBuffer, 
						(m_BlockSizeBytes) * (MAX_BUFFER_COUNT+1));

		VirtualFree(m_pfAudioBuffer, 0, MEM_RELEASE);

		m_pfAudioBuffer = NULL;
	}

	if(m_pSourceVoice)
	{
		HRESULT hRes = m_pSourceVoice->DestroyVoice();
		if(!SUCCEEDED(hRes))
		{
			MessageBox(NULL, TEXT("Error Destroying Voice"), TEXT("This is pretty serious"), MB_OK);
		}

		m_pSourceVoice = NULL;
	}

    // All XAudio2 interfaces are released when the engine is destroyed, but being tidy
	if(m_pMasteringVoice)
	{
	    m_pMasteringVoice->DestroyVoice();
		m_pMasteringVoice = NULL;
	}

	SAFE_RELEASE(m_pXAudio);

	return(true);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//		Setup Stuff, can only be called before initialize
//
//

bool CAudioOutput::SetFormat(unsigned long SampleRate, unsigned long Channels)
{
	if( (SampleRate != m_waveFormatPCMEx.Format.nSamplesPerSec) ||
		(Channels != m_waveFormatPCMEx.Format.nChannels) )
	{
		Shutdown();

		DWORD speakerconfig;

		if(Channels == 1)
		{
			speakerconfig = KSAUDIO_SPEAKER_MONO;
		}
		else if(Channels == 2)
		{
			speakerconfig = KSAUDIO_SPEAKER_STEREO;
		}
		else if(Channels == 4)
		{
			speakerconfig = KSAUDIO_SPEAKER_QUAD;
		}
		else if(Channels == 5)
		{
			speakerconfig = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT);
		}
		else if(Channels == 6)
		{
			speakerconfig = KSAUDIO_SPEAKER_5POINT1;
		}
		else
		{
			speakerconfig = 0;
		}

		m_waveFormatPCMEx.Format.cbSize					= 22;
		m_waveFormatPCMEx.Format.wFormatTag				= WAVE_FORMAT_EXTENSIBLE;
		m_waveFormatPCMEx.Format.nChannels				= (WORD)Channels;
		m_waveFormatPCMEx.Format.nSamplesPerSec			= SampleRate;
		m_waveFormatPCMEx.Format.wBitsPerSample			= 32;
		m_waveFormatPCMEx.Format.nBlockAlign			= (m_waveFormatPCMEx.Format.wBitsPerSample/8) * m_waveFormatPCMEx.Format.nChannels;
		m_waveFormatPCMEx.Format.nAvgBytesPerSec		= ((m_waveFormatPCMEx.Format.wBitsPerSample/8) * m_waveFormatPCMEx.Format.nChannels) * m_waveFormatPCMEx.Format.nSamplesPerSec; //Compute using nBlkAlign * nSamp/Sec 

		m_waveFormatPCMEx.Samples.wValidBitsPerSample	= 32;
		m_waveFormatPCMEx.Samples.wReserved				= 0;
		m_waveFormatPCMEx.Samples.wSamplesPerBlock		= 0;
		
		m_waveFormatPCMEx.dwChannelMask					= speakerconfig; 
		m_waveFormatPCMEx.SubFormat						= KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	
		unsigned long samplesperms = ((unsigned long)((float)m_waveFormatPCMEx.Format.nSamplesPerSec / 1000.0f)) * m_waveFormatPCMEx.Format.nChannels;

		m_BlockSize			=	samplesperms	* (BUFFERSIZEMS / MAX_BUFFER_COUNT);												// so 3*100ms buffers pls
		m_BlockSizeBytes	=	m_BlockSize		* sizeof(float);

		m_Interval = (BUFFERSIZEMS / MAX_BUFFER_COUNT);

		if(!Initialize())
		{
			MessageBox(NULL, TEXT("Initialize() Failed - No Sound!"), TEXT("XAudio Error..."), MB_OK);
			return false;
		}
	}

	return true;
}


////////////////////////////////////////////////////////////////////////////////////////////
// PLAYBACK CONTROLS
//
//
bool CAudioOutput::Start(void)
{	
	CAutoLock lock(&m_AudioLock);

	m_bPlaying = true;
	m_pSourceVoice->Start( 0, 0 );

	m_ulLastTickCount = GetTickCount();

	return true;
}

bool CAudioOutput::Stop(void)
{
	CAutoLock lock(&m_AudioLock);
	m_pSourceVoice->Stop(0);
	m_bPlaying = false;

	return true;
}

bool CAudioOutput::Reset(void)
{
	CAutoLock lock(&m_AudioLock);

	m_pSourceVoice->Stop(0);

	m_pSourceVoice->FlushSourceBuffers();

	m_pSourceVoice->Start( 0, 0 );

	return true;
}

__int64 CAudioOutput::GetSamplesOut(void)
{
	XAUDIO2_VOICE_STATE state;
	m_pSourceVoice->GetState( &state );

	return state.SamplesPlayed;
}

bool CAudioOutput::GetVisData(float * ToHere, unsigned long ulNumSamples)
{
	unsigned long samplesperms = ((unsigned long)((float)m_waveFormatPCMEx.Format.nSamplesPerSec / 1000.0f)) * m_waveFormatPCMEx.Format.nChannels;

	unsigned long ulThisTickCount = GetTickCount();
	unsigned long msDif = ulThisTickCount - m_ulLastTickCount;
	
	unsigned long offset = min(msDif, m_BlockSize) * samplesperms;

	if(m_bPlaying)
	{
		CopyFloat(	ToHere,
					&m_pfAudioBuffer[(m_BufferInProgress*m_BlockSize) + offset], 
					ulNumSamples);
	}

	return true;
}
