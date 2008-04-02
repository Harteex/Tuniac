#include "StdAfx.h"

#include "wavdecoder.h"

bool CWAVDecoder::FOURCC_EQUAL(char* X, char* Y)
{
	if(X[0]==Y[0] && X[1]==Y[1] && X[2]==Y[2] && X[3]==Y[3])
		return true;

	return false;
}

CWAVDecoder::CWAVDecoder(void)
{
}

CWAVDecoder::~CWAVDecoder(void)
{
}

bool CWAVDecoder::Open(LPTSTR szSource)
{
    m_file = _wfopen(szSource,TEXT("rbS"));
    if (m_file)
    {
		wstat = fread(&wav,sizeof(WAV_HDR),1,m_file);

		//check for RIFF header
		if(!FOURCC_EQUAL(wav.rID, "RIFF"))
			return false;

		//check for WAVE header
		if(!FOURCC_EQUAL(wav.wID, "WAVE"))
			return false;

		//check for fmt header
		if(!FOURCC_EQUAL(wav.fId, "fmt "))
			return false;

		m_bFloatMode = 0;
		if(wav.wFormatTag == 3)
			m_bFloatMode = 1;
		else if(wav.wFormatTag!=1)
			//unsupported format
			return false;

		rmore = wav.pcm_header_len - (sizeof(WAV_HDR) - 20);
		wstat = fseek(m_file,rmore,SEEK_CUR);
		if(wstat!=0)
			//cant seek
			return false;
	}

	if(wav.nBitsPerSample == 8)
	{
		m_divider = 128.0f;
	}
	else if(wav.nBitsPerSample == 12)
	{
		m_divider = 4095.0f;
	}
	else if(wav.nBitsPerSample == 16)
	{
		m_divider = 32767.0f;
	}
	else if(wav.nBitsPerSample == 20)
	{
		m_divider = 1048575.0f;
	}
	else if(wav.nBitsPerSample == 24)
	{
		m_divider = 8388608.0f;
	}
	else if(wav.nBitsPerSample == 32)
	{
		m_divider = 2147483648.0;
	}


	return(true);
}

bool CWAVDecoder::Close()
{
	return(true);
}

void		CWAVDecoder::Destroy(void)
{
	Close();
	delete this;
}

bool		CWAVDecoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels)
{
	*SampleRate = wav.nSamplesPerSec;
	*Channels	= wav.nChannels;

	return(true);
}

bool		CWAVDecoder::GetLength(unsigned long * MS)
{
	//*MS = filesize / (unsigned long)wav.nAvgBytesPerSec;wav.nSamplesPerSec  wav.nBitsPerSample wav.nSamplesPerSec

	//int x =0;
	return(true);
}

bool		CWAVDecoder::SetPosition(unsigned long * MS)
{
	return(true);
}

bool		CWAVDecoder::SetState(unsigned long State)
{
	return(true);
}

bool		CWAVDecoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples)
{
	*NumSamples =0;

	// read chunks until a 'data' chunk is found
	sflag = 1;
	while(sflag!=0){

		// check attempts before it gets out of hand
		if(sflag>10)
			//too many chunks
			return false;

		 // read chunk header
		wstat = fread(&chk,sizeof(CHUNK_HDR),(size_t)1,m_file);
		if(wstat!=1)
			//cant read chunk
			return false;

		// check chunk type
			if(FOURCC_EQUAL(chk.dId, "data"))
				break;
	 
		// skip over chunk
		sflag++;
		wstat = fseek(m_file,chk.dLen,SEEK_CUR);
		if(wstat!=0)
			//cant seek
			return false;
	}

	/* find length of remaining data */ 
	wbuff_len = chk.dLen;

	/* read signal data */
	wstat = fread(wbuff,wbuff_len,1,m_file);
	if(wstat!=1)
		//cant read chunk
		return false;

	if(m_bFloatMode)
	{
		*ppBuffer = (float*)wbuff;
	}
	else
	{
		int		* pData		= (int *)wbuff;
		float	* pBuffer	= m_Buffer;
		
		for(int x=0; x<wbuff_len;x++)
		{
			*pBuffer = (float) ((double)(*pData) / m_divider);	

			pData ++;
			pBuffer++;
		}
		*ppBuffer = m_Buffer;
	}

	*NumSamples = wbuff_len;

	return(true);
}