#include "global.h"
#include "RageSoundReader_MP3.h"
#include "RageLog.h"
#include "RageUtil.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <map>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

RageSoundReader_MP3::RageSoundReader_MP3()
	: formatContext(nullptr), codecContext(nullptr), frame(nullptr), packet(nullptr), audioStreamIndex(-1), nextPts(0) {
}

RageSoundReader_MP3::~RageSoundReader_MP3() {
	Close();
}

RageSoundReader_FileReader::OpenResult RageSoundReader_MP3::Open(RageFileBasic* pFile) {
	m_pFile = pFile;

	// Open the file using FFmpeg
	if (avformat_open_input(&formatContext, m_pFile->GetDisplayPath().c_str(), nullptr, nullptr) < 0) {
		SetError("Failed to open MP3 file with FFmpeg");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Retrieve stream information
	if (avformat_find_stream_info(formatContext, nullptr) < 0) {
		SetError("Failed to find stream information");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Find the audio stream
	audioStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (audioStreamIndex < 0) {
		SetError("Failed to find audio stream");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Get the codec context
	AVStream* audioStream = formatContext->streams[audioStreamIndex];
	codecContext = avcodec_alloc_context3(nullptr);
	if (!codecContext) {
		SetError("Failed to allocate codec context");
		return OPEN_FATAL_ERROR;
	}

	if (avcodec_parameters_to_context(codecContext, audioStream->codecpar) < 0) {
		SetError("Failed to copy codec parameters to codec context");
		return OPEN_FATAL_ERROR;
	}

	// Find and open the codec
	AVCodec* codec = avcodec_find_decoder(codecContext->codec_id);
	if (!codec) {
		SetError("Failed to find codec");
		return OPEN_FATAL_ERROR;
	}

	if (avcodec_open2(codecContext, codec, nullptr) < 0) {
		SetError("Failed to open codec");
		return OPEN_FATAL_ERROR;
	}

	// Allocate frame and packet
	frame = av_frame_alloc();
	if (!frame) {
		SetError("Failed to allocate frame");
		return OPEN_FATAL_ERROR;
	}

	packet = av_packet_alloc();
	if (!packet) {
		SetError("Failed to allocate packet");
		return OPEN_FATAL_ERROR;
	}

	return OPEN_OK;
}

RString RageSoundReader_MP3::GetError() const {
	return m_sError;
}

void RageSoundReader_MP3::SetError(RString sError) const {
	m_sError = sError;
}

void RageSoundReader_MP3::Close() {
	if (packet) {
		av_packet_free(&packet);
	}
	if (frame) {
		av_frame_free(&frame);
	}
	if (codecContext) {
		avcodec_free_context(&codecContext);
	}
	if (formatContext) {
		avformat_close_input(&formatContext);
	}
	m_pFile = nullptr;
}

int RageSoundReader_MP3::Read(float* buf, int iFrames) {
	int iFramesWritten = 0;
	int bytesPerSample = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);
	int bufferSize = iFrames * Channels * bytesPerSample;
	uint8_t* outputBuffer = reinterpret_cast<uint8_t*>(buf);

	while (iFramesWritten < iFrames) {
		if (av_read_frame(formatContext, packet) < 0) {
			return END_OF_FILE;
		}

		if (packet->stream_index == audioStreamIndex) {
			if (avcodec_send_packet(codecContext, packet) < 0) {
				SetError("Error sending packet to decoder");
				return -1;
			}

			while (avcodec_receive_frame(codecContext, frame) == 0) {
				int dataSize = av_samples_get_buffer_size(nullptr, codecContext->channels, frame->nb_samples, AV_SAMPLE_FMT_FLT, 1);
				if (dataSize < 0) {
					SetError("Failed to calculate data size");
					return -1;
				}

				if (iFramesWritten + frame->nb_samples > iFrames) {
					dataSize = (iFrames - iFramesWritten) * Channels * bytesPerSample;
				}

				memcpy(outputBuffer, frame->data[0], dataSize);
				outputBuffer += dataSize;
				iFramesWritten += frame->nb_samples;
			}
		}

		av_packet_unref(packet);
	}

	return iFramesWritten;
}


int RageSoundReader_MP3::GetNextSourceFrame() const {
	return static_cast<int>(nextPts);
}

bool RageSoundReader_MP3::SetProperty(const RString& sProperty, float fValue) {
	return RageSoundReader_FileReader::SetProperty(sProperty, fValue);
}

int RageSoundReader_MP3::GetLength() const {
	if (!formatContext) {
		SetError("Format context is not initialized");
		return ERROR;
	}

	int64_t duration = formatContext->duration;
	if (duration == AV_NOPTS_VALUE) {
		SetError("Error getting length of MP3 file");
		return ERROR;
	}

	return static_cast<int>(duration / (AV_TIME_BASE / 1000)); // Convert to milliseconds
}

int RageSoundReader_MP3::SetPosition(int iFrame) {
	if (!formatContext) {
		SetError("Format context is not initialized");
		return ERROR;
	}

	// Convert the frame position to a timestamp
	int64_t timestamp = av_rescale_q(iFrame, { 1, SampleRate }, formatContext->streams[audioStreamIndex]->time_base);

	// Seek to the specified frame
	if (av_seek_frame(formatContext, audioStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
		SetError("Error seeking to the specified frame");
		return ERROR;
	}

	// Flush the codec context to clear any buffered data
	avcodec_flush_buffers(codecContext);

	// Reset the nextPts to the new position
	nextPts = iFrame;

	return 0; // Return 0 to indicate success
}

RString RageSoundReader_MP3::GetError() const {
	return m_sError;
}

float RageSoundReader_MP3::GetStreamToSourceRatio() const {
	// Implementation of the GetStreamToSourceRatio function
	return 0.0f; // Placeholder return value
}
int RageSoundReader_MP3::GetSampleRate() const {
	return SampleRate;
}
