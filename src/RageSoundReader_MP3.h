#ifndef RAGE_SOUND_READER_MP3_H
#define RAGE_SOUND_READER_MP3_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

#include "RageSoundReader_FileReader.h"
#include "RageFile.h"

class RageSoundReader_MP3 : public RageSoundReader_FileReader {
public:
	RageSoundReader_MP3();
	~RageSoundReader_MP3();
	OpenResult Open(RageFileBasic* pFile) override;
	void Close();
	int Read(float* buf, int iFrames) override;
	int GetNextSourceFrame() const override;
	int GetLength() const override;
	int SetPosition(int iFrame) override;
	unsigned GetNumChannels() const override;
	int GetSampleRate() const override;
	bool SetProperty(const RString& sProperty, float fValue) override;
	RString GetError() const override;
	float GetStreamToSourceRatio() const override;

private:
void SetError(const RString& error);
	AVFormatContext* formatContext;
	AVCodecContext* codecContext;
	AVFrame* frame;
	AVPacket* packet;
	//SwrContext* swrContext;
	int audioStreamIndex;
	int64_t nextPts;
	int SampleRate;
	unsigned Channels;
	RageFileBasic* m_pFile;
	mutable RString m_sError;
};

#endif // RAGE_SOUND_READER_MP3_H
