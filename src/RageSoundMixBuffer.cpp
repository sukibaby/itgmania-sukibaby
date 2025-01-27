#include "global.h"
#include "RageSoundMixBuffer.h"
#include "RageUtil.h"

#include <cmath>
#include <cstdint>
#include <vector>

RageSoundMixBuffer::RageSoundMixBuffer() {
	buf_size_ = buf_used_ = 0;
	offset_ = 0;
}

RageSoundMixBuffer::~RageSoundMixBuffer() {
}

void RageSoundMixBuffer::extend(int64_t samples) {
	const int64_t realsize = samples + offset_;
	if (buf_size_ < realsize) {
		mixbuf_.resize(realsize);
		buf_size_ = realsize;
	}

	if (buf_used_ < realsize) {
		std::fill(mixbuf_.begin() + buf_used_, mixbuf_.begin() + realsize, 0.0f);
		buf_used_ = realsize;
	}
}

void RageSoundMixBuffer::write(const float* buf, int64_t size, int source_stride, int dest_stride) {
	if (size == 0)
		return;

	/* size = 3, dest_stride = 2 uses 4 frames.  Don't allocate the stride of the
	 * last sample. */
	extend(size * dest_stride - (dest_stride - 1));

	/* Scale volume and add. */
	float* dest_buf = mixbuf_.data() + offset_;

	while (size) {
		*dest_buf += *buf;
		buf += source_stride;
		dest_buf += dest_stride;
		--size;
	}
}

void RageSoundMixBuffer::read(int16_t* buf) {
	for (int64_t pos = 0; pos < buf_used_; ++pos) {
		float out = mixbuf_[pos];
		out = std::clamp(out, -1.0f, +1.0f);
		buf[pos] = std::lrint(out * INT16_MAX);
	}
	buf_used_ = 0;
}

void RageSoundMixBuffer::read(float* buf) {
	std::copy(mixbuf_.begin(), mixbuf_.begin() + buf_used_, buf);
	buf_used_ = 0;
}

void RageSoundMixBuffer::read_deinterlace(float** bufs, int channels) {
	for (int64_t i = 0; i < buf_used_ / channels; ++i) {
		for (int ch = 0; ch < channels; ++ch) {
			bufs[ch][i] = mixbuf_[channels * i + ch];
		}
	}
	buf_used_ = 0;
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
