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
//#include <libswresample/swresample.h>
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
	const AVCodec* codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
	if (!codec) {
		SetError("Failed to find codec");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	codecContext = avcodec_alloc_context3(codec);
	if (avcodec_parameters_to_context(codecContext, audioStream->codecpar) < 0) {
		SetError("Failed to copy codec parameters to codec context");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	if (avcodec_open2(codecContext, codec, nullptr) < 0) {
		SetError("Failed to open codec");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	//// Initialize the resampler
	//swrContext = swr_alloc();
	//if (!swrContext) {
	//	SetError("Failed to allocate resampler context");
	//	return OPEN_UNKNOWN_FILE_FORMAT;
	//}

	//av_opt_set_int(swrContext, "in_channel_layout", codecContext->channel_layout, 0);
	//av_opt_set_int(swrContext, "in_sample_rate", codecContext->sample_rate, 0);
	//av_opt_set_sample_fmt(swrContext, "in_sample_fmt", codecContext->sample_fmt, 0);
	//av_opt_set_int(swrContext, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	//av_opt_set_int(swrContext, "out_sample_rate", codecContext->sample_rate, 0);
	//av_opt_set_sample_fmt(swrContext, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

	//if (swr_init(swrContext) < 0) {
	//	SetError("Failed to initialize resampler");
	//	return OPEN_UNKNOWN_FILE_FORMAT;
	//}

	frame = av_frame_alloc();
	packet = av_packet_alloc();

	SampleRate = codecContext->sample_rate;
	Channels = 2; // We are converting to stereo

	return OPEN_OK;
}

void RageSoundReader_MP3::Close() {
	//if (packet) {
	//	av_packet_free(&packet);
	//	packet = nullptr;
	//}
	//if (frame) {
	//	av_frame_free(&frame);
	//	frame = nullptr;
	//}
	////if (swrContext) {
	////	swr_free(&swrContext);
	////	swrContext = nullptr;
	////}
	//if (codecContext) {
	//	avcodec_free_context(&codecContext);
	//	codecContext = nullptr;
	//}
	//if (formatContext) {
	//	avformat_close_input(&formatContext);
	//	formatContext = nullptr;
	//}
	//if (m_pFile) {
	//	//m_pFile->Close();
	//	m_pFile = nullptr;
	//}
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
				return ERROR;
			}

			//while (avcodec_receive_frame(codecContext, frame) == 0) {
			//	int outputSamples = swr_convert(swrContext, &outputBuffer, bufferSize, (const uint8_t**)frame->data, frame->nb_samples);
			//	if (outputSamples < 0) {
			//		SetError("Error converting samples");
			//		return ERROR;
			//	}

				//int framesRead = outputSamples / Channels;
				//iFramesWritten += framesRead;
				//outputBuffer += outputSamples * bytesPerSample;
				//bufferSize -= outputSamples * bytesPerSample;
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
