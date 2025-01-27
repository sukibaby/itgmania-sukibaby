#include "global.h"
#include "RageSoundMixBuffer.h"
#include "RageUtil.h"

#include <cmath>
#include <cstdint>
#include <vector>

RageSoundMixBuffer::RageSoundMixBuffer()
{
	m_iBufSize = m_iBufUsed = 0;
	m_iOffset = 0;
}

RageSoundMixBuffer::~RageSoundMixBuffer()
{
}

/* write() will start mixing iOffset samples into the buffer.  Be careful; this is
 * measured in samples, not frames, so if the data is stereo, multiply by two. */
void RageSoundMixBuffer::SetWriteOffset(int iOffset)
{
	m_iOffset = iOffset;
}

void RageSoundMixBuffer::Extend(std::int64_t iSamples)
{
	const std::int64_t realsize = iSamples + m_iOffset;
	if (m_iBufSize < realsize)
	{
		m_pMixbuf.resize(realsize);
		m_iBufSize = realsize;
	}

	if (m_iBufUsed < realsize)
	{
		std::fill(m_pMixbuf.begin() + m_iBufUsed, m_pMixbuf.begin() + realsize, 0.0f);
		m_iBufUsed = realsize;
	}
}

void RageSoundMixBuffer::write(const float* pBuf, int64_t iSize, int iSourceStride, int iDestStride)
{
	if (iSize == 0)
		return;

	/* iSize = 3, iDestStride = 2 uses 4 frames.  Don't allocate the stride of the
	 * last sample. */
	Extend(iSize * iDestStride - (iDestStride - 1));

	/* Scale volume and add. */
	float* pDestBuf = m_pMixbuf.data() + m_iOffset;

	while (iSize)
	{
		*pDestBuf += *pBuf;
		pBuf += iSourceStride;
		pDestBuf += iDestStride;
		--iSize;
	}
}

void RageSoundMixBuffer::read(std::int16_t* pBuf)
{
	for (std::int64_t iPos = 0; iPos < m_iBufUsed; ++iPos)
	{
		float iOut = m_pMixbuf[iPos];
		iOut = std::clamp(iOut, -1.0f, +1.0f);
		pBuf[iPos] = std::lrint(iOut * INT16_MAX);
	}
	m_iBufUsed = 0;
}

void RageSoundMixBuffer::read(float* pBuf)
{
	std::copy(m_pMixbuf.begin(), m_pMixbuf.begin() + m_iBufUsed, pBuf);
	m_iBufUsed = 0;
}

void RageSoundMixBuffer::read_deinterlace(float** pBufs, int channels)
{
	for (std::int64_t i = 0; i < m_iBufUsed / channels; ++i)
	{
		for (int ch = 0; ch < channels; ++ch)
		{
			pBufs[ch][i] = m_pMixbuf[channels * i + ch];
		}
	}
	m_iBufUsed = 0;
}

/*
 * Copyright (c) 2002-2004 Glenn Maynard
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
