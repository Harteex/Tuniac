#pragma once
#include "iinfomanager.h"

#include "tag.h"
#include "tfile.h"
#include "fileref.h"
#include "mpegfile.h"
#include "mpcfile.h"
#include "trueaudiofile.h"
#include "wavpackfile.h"
#include "flacfile.h"
#include "vorbisfile.h"
#include "speexfile.h"
#include "oggflacfile.h"
#include "mp4file.h"
#include "asffile.h"
#include "aifffile.h"
#include "wavfile.h"

#include "id3v1tag.h"
#include "id3v2tag.h"
#include "attachedpictureframe.h"
#include "relativevolumeframe.h"
#include "apetag.h"
#include "xiphcomment.h"
#include "mp4tag.h"
#include "mp4item.h"
#include "mp4atom.h"

class CSTDInfoManager :
	public IInfoManager
{
protected:
	TagLib::FileRef fileref;
	TagLib::FLAC::File *flacfile;
	TagLib::MPEG::File *mp3file;
	TagLib::MP4::File *mp4file;
	TagLib::MPC::File *mpcfile;
	TagLib::TrueAudio::File *ttafile;
	TagLib::WavPack::File *wvfile;
	TagLib::Ogg::Vorbis::File *oggfile;
	TagLib::Ogg::FLAC::File *ogafile;
	TagLib::Ogg::Speex::File *spxfile;
	TagLib::ASF::File *wmafile;
	TagLib::RIFF::AIFF::File *aiffile;
	TagLib::RIFF::WAV::File *wavfile;

public:
	CSTDInfoManager(void);
	~CSTDInfoManager(void);

public:
	void			Destroy(void);

	unsigned long	GetNumExtensions(void);
	LPTSTR			SupportedExtension(unsigned long ulExtentionNum);

	bool			CanHandle(LPTSTR szSource);
	bool			GetInfo(LibraryEntry * libEnt);
	bool			SetInfo(LibraryEntry * libEnt);

	unsigned long	GetNumberOfAlbumArts(LPTSTR		szFilename);
	bool			GetAlbumArt(LPTSTR				szFilename, 
								unsigned long		ulImageIndex,
								LPVOID			*	pImageData,
								unsigned long	*	ulImageDataSize,
								LPTSTR				szMimeType,
								unsigned long	*	ulArtType);

	bool			FreeAlbumArt(LPVOID				pImageData);
};
