/* MovieTexture_FFMpeg - FFMpeg movie renderer. */

#ifndef RAGE_MOVIE_TEXTURE_FFMPEG_H
#define RAGE_MOVIE_TEXTURE_FFMPEG_H

#include "MovieTexture_Generic.h"

#include <cstdint>
#include <mutex>

struct RageSurface;

namespace avcodec
{
	extern "C"
	{
		#include <libavcodec/avcodec.h>
		#include <libavformat/avformat.h>
		#include <libswscale/swscale.h>
		#include <libavutil/pixdesc.h>
	}
};

#define STEPMANIA_FFMPEG_BUFFER_SIZE 4096
static const int sws_flags = SWS_BICUBIC; // XXX: Reasonable default?

struct FrameHolder {
	avcodec::AVFrame frame;
	avcodec::AVPacket packet;
	float frameTimestamp;
	float frameDelay;
	bool decoded = false;
	bool skip = false;
	std::mutex lock; // Protects the frame as it's being initialized.

	FrameHolder() = default;

	// Copy constructor, unused but we need to make the compiler not copy
	// the mutex.
	FrameHolder(const FrameHolder& fh) {
		frame = fh.frame;
		packet = fh.packet;
		frameTimestamp = fh.frameTimestamp;
		frameDelay = fh.frameDelay;
		decoded = fh.decoded;
		skip = fh.skip;
	}
};

class MovieTexture_FFMpeg: public MovieTexture_Generic
{
public:
	MovieTexture_FFMpeg( RageTextureID ID );

	static RageSurface *AVCodecCreateCompatibleSurface( int iTextureWidth, int iTextureHeight, bool bPreferHighColor, int &iAVTexfmt, MovieDecoderPixelFormatYCbCr &fmtout );
};

class RageMovieTextureDriver_FFMpeg: public RageMovieTextureDriver
{
public:
	virtual RageMovieTexture *Create( RageTextureID ID, RString &sError );
	static RageSurface *AVCodecCreateCompatibleSurface( int iTextureWidth, int iTextureHeight, bool bPreferHighColor, int &iAVTexfmt, MovieDecoderPixelFormatYCbCr &fmtout );
};

class MovieDecoder_FFMpeg: public MovieDecoder
{
public:
	MovieDecoder_FFMpeg();
	~MovieDecoder_FFMpeg();

	RString Open( RString sFile );
	void Close();
	void Rewind();

	// This draws a frame from the buffer onto the provided RageSurface.
	// Returns true if returning the last frame in the movie.
	bool GetFrame(RageSurface* pOut);
	int DecodeFrame( float fTargetTime );

	// Decode a single frame.  Return -2 on cancel, -1 on error, 0 on EOF, 1 if we have a frame.
	int DecodeNextFrame();

	// Decode the entire movie.
	// If we let this decode as fast as possible, it could come at the expense
	// of gameplay performance. Since dropping frames is undesirable, an
	// artificial rate limit is introduced.
	//
	// Given that most movies will display at 30fps or 60fps, decoding at a
	// speed of 1000fps should be more than sufficient to ensure we never
	// run behind. This rate limiting is only needed in case itgmania is
	// running on a low performance machine, or the movie is REALLY long.
	//
	// Returns 0 on success, -1 on fatal error, -2 on cancel.
	int DecodeMovie();
	bool IsCurrentFrameReady();

	int GetWidth() const { return m_pStreamCodec->width; }
	int GetHeight() const { return m_pStreamCodec->height; }

	RageSurface *CreateCompatibleSurface( int iTextureWidth, int iTextureHeight, bool bPreferHighColor, MovieDecoderPixelFormatYCbCr &fmtout );

	float GetTimestamp() const;

	void Cancel() { cancel = true; };

	// If the next frame to display had an issue decoding, skip it.
	bool SkipNextFrame();

private:
	void Init();
	RString OpenCodec();

	// Read a packet and send it to our frame data buffer.
	// Returns -2 on cancel, -1 on error, 0 on EOF, 1 on OK.
	int SendPacketToBuffer();

	// Decode frame data from the packet in the buffer.
	// Returns -2 on cancel, -1 on error, 0 if the packet is finished.
	int DecodePacketInBuffer();

	avcodec::AVStream *m_pStream;
	avcodec::AVFrame *m_Frame;
	avcodec::AVPixelFormat m_AVTexfmt;	/* pixel format of output surface */
	avcodec::SwsContext *m_swsctx;
	avcodec::AVCodecContext *m_pStreamCodec;

	avcodec::AVFormatContext *m_fctx;
	float m_fTimestamp;
	float m_fTimestampOffset;
	int m_iFrameNumber;
	int m_totalFrames; // Total number of frames in the movie.

	unsigned char *m_buffer;
	avcodec::AVIOContext *m_avioContext;

	// The movie buffer.
	std::vector<FrameHolder> m_FrameBuffer;

	int m_iCurrentPacketOffset;

	// 0 = no EOF
	// 1 = EOF while decoding
	int m_iEOF;

	// If true, received a cancel signal from the MovieTexture.
	bool cancel = false;
	bool firstFrame = true;
};

static struct AVPixelFormat_t
{
	int bpp;
	std::uint32_t masks[4];
	avcodec::AVPixelFormat pf;
	bool bHighColor;
	bool bByteSwapOnLittleEndian;
	MovieDecoderPixelFormatYCbCr YUV;
} AVPixelFormats[] = {
	{
		32,
		{ 0xFF000000,
		  0x00FF0000,
		  0x0000FF00,
		  0x000000FF },
		avcodec::AV_PIX_FMT_YUYV422,
		false, /* N/A */
		true,
		PixelFormatYCbCr_YUYV422,
	},
	{
		32,
		{ 0x0000FF00,
		  0x00FF0000,
		  0xFF000000,
		  0x000000FF },
		avcodec::AV_PIX_FMT_BGRA,
		true,
		true,
		PixelFormatYCbCr_Invalid,
	},
	{
		32,
		{ 0x00FF0000,
		  0x0000FF00,
		  0x000000FF,
		  0xFF000000 },
		avcodec::AV_PIX_FMT_ARGB,
		true,
		true,
		PixelFormatYCbCr_Invalid,
	},
	/*
	{
		32,
		{ 0x000000FF,
		  0x0000FF00,
		  0x00FF0000,
		  0xFF000000 },
		avcodec::AV_PIX_FMT_ABGR,
		true,
		true,
		PixelFormatYCbCr_Invalid,
	},
	{
		32,
		{ 0xFF000000,
		  0x00FF0000,
		  0x0000FF00,
		  0x000000FF },
		avcodec::AV_PIX_FMT_RGBA,
		true,
		true,
		PixelFormatYCbCr_Invalid,
	}, */
	{
		24,
		{ 0xFF0000,
		  0x00FF00,
		  0x0000FF,
		  0x000000 },
		avcodec::AV_PIX_FMT_RGB24,
		true,
		true,
		PixelFormatYCbCr_Invalid,
	},
	{
		24,
		{ 0x0000FF,
		  0x00FF00,
		  0xFF0000,
		  0x000000 },
		avcodec::AV_PIX_FMT_BGR24,
		true,
		true,
		PixelFormatYCbCr_Invalid,
	},
	{
		16,
		{ 0x7C00,
		  0x03E0,
		  0x001F,
		  0x0000 },
		avcodec::AV_PIX_FMT_RGB555,
		false,
		false,
		PixelFormatYCbCr_Invalid,
	},
	{ 0, { 0,0,0,0 }, avcodec::AV_PIX_FMT_NB, true, false, PixelFormatYCbCr_Invalid }
};

#endif

/*
 * (c) 2003-2005 Glenn Maynard
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
