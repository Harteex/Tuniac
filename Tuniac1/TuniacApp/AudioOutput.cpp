#include "StdAfx.h"
#include ".\audiooutput.h"

#define CopyFloat(dst, src, num) CopyMemory(dst, src, (num) * sizeof(float))

#define MAX_BUFFER_COUNT		10


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

CAudioOutput::CAudioOutput(IXAudio2 * pXAudio, unsigned long ulBufferSize) : 
	m_hThread(NULL),
	m_hEvent(NULL),
	m_pSourceVoice(NULL),
	m_pfAudioBuffer(NULL),
	m_pCallback(NULL),
	m_BufferInProgress(0),
	m_pXAudio(pXAudio)
{
	ZeroMemory(&m_waveFormatPCMEx, sizeof(m_waveFormatPCMEx));
	m_pfAudioBuffer = NULL;
	m_ulBuffersizeMS = ulBufferSize;
}

CAudioOutput::~CAudioOutput(void)
{
}

void CAudioOutput::Destroy()
{
	SetCallback(NULL);
	Shutdown();
	delete this;
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
	DWORD currentDiskReadBuffer = 0;
	while(m_bThreadRun)
	{
		float * pOffset = &m_pfAudioBuffer[(currentDiskReadBuffer*m_BlockSize)];

		if(m_pCallback && m_bPlaying)
		{

			if(m_pCallback->GetBuffer(pOffset, m_BlockSize))
			{
				CAutoLock lock(&m_AudioLock);

				// HACK: this is highly inefficient and must be fixed!
				// what this hack does, is to copy the first bufer, PAST the end of the 'last buffer'
				// we actually allocate 'n'+1 buffers for this reason
				// this means, when we want to copy visuals out, we can just do a single memcopy
				// instead of two, this is probably more efficient anyway?
				// if I could do so in windows, I'd just memmap the block of data after itself,
				// so it would all be virtual and let the MMU deal with it
				// BUT I CAN'T :((((((
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
		///////////////////////////////////////////////////////////////////////
		//
		// ok we wait here while we have no buffers available
		//
		// While we wait we see if we can service the stream to make sure 
		// we have a buffer ready to go when a buffer becomes available!
		// this way, we dont have to suddenly jump up, do a crapload of 
		// decoding then write the buffer a simple memory copy will suffice!
		//
        XAUDIO2_VOICE_STATE state;
        while( (m_pSourceVoice->GetState( &state ), state.BuffersQueued >= MAX_BUFFER_COUNT) && m_bThreadRun)
        {
			if(!m_pCallback->ServiceStream())
			{
				// if the stream didn't need servicing then we have a full buffer ready for when we 
				// are able to write to the audio device. So.. lets go to sleep!
				// we'll be awoken by the audio device, but lets sleep for the length of one buffer
				WaitForSingleObject( m_hEvent, m_Interval );
			}
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

	HRESULT chr = m_pXAudio->CreateSourceVoice(	&m_pSourceVoice, 
												(const WAVEFORMATEX*)&m_waveFormatPCMEx, 
												XAUDIO2_VOICE_MUSIC, 
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
		m_bPlaying = false;
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
		m_pSourceVoice->DestroyVoice();
		m_pSourceVoice = NULL;
	}

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

		m_BlockSize			=	samplesperms	* (m_ulBuffersizeMS / MAX_BUFFER_COUNT);												// so 3*100ms buffers pls
		m_BlockSizeBytes	=	m_BlockSize		* sizeof(float);

		m_Interval = (m_ulBuffersizeMS / MAX_BUFFER_COUNT);

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

	if(m_pSourceVoice)
		m_pSourceVoice->Start( 0, 0 );

	m_ulLastTickCount = GetTickCount();

	return true;
}

bool CAudioOutput::Stop(void)
{
	CAutoLock lock(&m_AudioLock);
	if(m_pSourceVoice)
		m_pSourceVoice->Stop(0);
	m_bPlaying = false;

	return true;
}

bool CAudioOutput::Reset(void)
{
	CAutoLock lock(&m_AudioLock);
	if(m_pSourceVoice)
	{
		m_pSourceVoice->Stop(0);
		m_pSourceVoice->FlushSourceBuffers();

		if(m_bPlaying)
			m_pSourceVoice->Start( 0, 0 );
	}
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

	// ok lets clamp offset so it can't gobeyond the end of the buffer

	offset = min(offset, m_BlockSize);

	ulNumSamples = min(m_BlockSize, ulNumSamples);

	if(m_bPlaying)
	{
		CopyFloat(	ToHere,
					&m_pfAudioBuffer[(m_BufferInProgress*m_BlockSize) + offset], 
					ulNumSamples);
	}

	return true;
}
