#ifndef RAGE_SOUND_READER_MP3_H
#define RAGE_SOUND_READER_MP3_H

#include "RageSoundReader_FileReader.h"
#include "RageFile.h"

class RageSoundReader_MP3: public RageSoundReader_FileReader
{
public:

	/* done */
	OpenResult Open( RageFileBasic *pFile );
	void Close(); // done
	int Read(float* pBuf, int iFrames);  // done
	int GetNextSourceFrame() const; // done
	bool SetProperty(const RString& sProperty, float fValue); // done (?)

	/* still working on it */
	int GetLength() const;
	int GetLength_Fast() const { return GetLength(); } // this should just be an alias of GetLength()
	int SetPosition( int iSample );
	unsigned GetNumChannels() const { return Channels; }
	int GetSampleRate() const { return SampleRate; }


	/* ughhhh!  */
	RageSoundReader_MP3();
	~RageSoundReader_MP3();
	RageSoundReader_MP3( const RageSoundReader_MP3 & ); /* not defined; don't use */
	RageSoundReader_MP3 *Copy() const;

private:
};

#endif

/*
 * Copyright (c) 2001-2004 Chris Danford, Glenn Maynard
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

