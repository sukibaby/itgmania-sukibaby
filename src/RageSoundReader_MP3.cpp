/* MAD is available from: http://www.underbit.com/products/mad/ */

#include "global.h"
#include "RageSoundReader_MP3.h"
#include "RageLog.h"
#include "RageUtil.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <map>

#include "mpg123.h"

RageSoundReader_MP3::RageSoundReader_MP3()
	: mh(nullptr), m_pFile(nullptr)
{
}

RageSoundReader_MP3::~RageSoundReader_MP3()
{
	if (mh != nullptr)
	{
		mpg123_close(mh);
		mpg123_delete(mh);
		mh = nullptr;
	}

	if (m_pFile != nullptr)
	{
		m_pFile->Close();
		m_pFile = nullptr;
	}
}

#include "mpg123.h"

RageSoundReader_FileReader::OpenResult RageSoundReader_MP3::Open(RageFileBasic* pFile)
{
	m_pFile = pFile;

	// Initialize mpg123 library
	if (mpg123_init() != MPG123_OK) {
		SetError("Failed to initialize mpg123");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Create a new mpg123 handle
	mpg123_handle* mh = mpg123_new(NULL, NULL);
	if (mh == NULL) {
		SetError("Failed to create mpg123 handle");
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Open the file with mpg123
	if (mpg123_open_handle(mh, m_pFile) != MPG123_OK) {
		SetError("Failed to open MP3 file with mpg123");
		mpg123_delete(mh);
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Read and decode the first frame to get header info
	unsigned char* audio = nullptr;
	size_t bytes = 0;
	int err = mpg123_read(mh, audio, 0, &bytes);
	if (err != MPG123_OK && err != MPG123_NEW_FORMAT) {
		SetError("Failed to read any data at all");
		mpg123_close(mh);
		mpg123_delete(mh);
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Get the format information
	long rate;
	int channels, encoding;
	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		SetError("Failed to get format information");
		mpg123_close(mh);
		mpg123_delete(mh);
		return OPEN_UNKNOWN_FILE_FORMAT;
	}

	// Store the sample rate and number of channels
	SampleRate = rate;
	this->Channels = channels;

	// Estimate the length of the file
	off_t length = mpg123_length(mh);
	if (length != MPG123_ERR) {
		this->Length = static_cast<int>((length * 1000) / rate); // Convert to milliseconds
	}
	else {
		this->Length = -1; // Unknown length
	}

	// Clean up
	mpg123_close(mh);
	mpg123_delete(mh);

	return OPEN_OK;
}

int RageSoundReader_MP3::Read(float* buf, int iFrames)
{
	int iFramesWritten = 0;
	size_t bytesToRead = iFrames * GetNumChannels() * sizeof(float);
	unsigned char* audioBuffer = reinterpret_cast<unsigned char*>(buf);
	size_t bytesRead = 0;

	while (iFrames > 0)
	{
		int err = mpg123_read(mh, audioBuffer, bytesToRead, &bytesRead);
		if (err == MPG123_DONE)
		{
			return END_OF_FILE;
		}
		else if (err != MPG123_OK)
		{
			SetError("Error reading MP3 data");
			return ERROR;
		}

		int samplesRead = bytesRead / sizeof(float);
		int framesRead = samplesRead / GetNumChannels();

		buf += samplesRead;
		iFrames -= framesRead;
		iFramesWritten += framesRead;
		audioBuffer += bytesRead;
		bytesToRead -= bytesRead;
	}

	return iFramesWritten;
}

bool RageSoundReader_MP3::SetProperty( const RString &sProperty, float fValue )
{
	return RageSoundReader_FileReader::SetProperty( sProperty, fValue );
}

int RageSoundReader_MP3::GetNextSourceFrame() const
{
	off_t frameOffset = mpg123_tellframe(mh);
	if (frameOffset < 0)
	{
		SetError("Error getting current frame position");
		return ERROR;
	}
	return static_cast<int>(frameOffset);
}

void RageSoundReader_MP3::Close()
{
	if (mh != nullptr)
	{
		mpg123_close(mh);
		mpg123_delete(mh);
		mh = nullptr;
	}

	if (m_pFile != nullptr)
	{
		m_pFile->Close();
		m_pFile = nullptr;
	}
}
