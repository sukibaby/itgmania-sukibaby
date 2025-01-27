#include "global.h"
#include "RageSoundReader_Merge.h"
#include "RageSoundReader_Resample_Good.h"
#include "RageSoundReader_Pan.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageSoundUtil.h"

#include <cmath>
#include <vector>
#include <cstring>

#include <thread>
#include <mutex>
#include <atomic>

RageSoundReader_Merge::RageSoundReader_Merge()
{
	m_iSampleRate = -1;
	m_iChannels = 0;
	m_iNextSourceFrame = 0;
	m_fCurrentStreamToSourceRatio = 1.0f;
}

RageSoundReader_Merge::~RageSoundReader_Merge()
{
	for (RageSoundReader* it : m_aSounds)
		delete it;
}

RageSoundReader_Merge::RageSoundReader_Merge(const RageSoundReader_Merge& cpy) :
	RageSoundReader(cpy)
{
	m_iSampleRate = cpy.m_iSampleRate;
	m_iChannels = cpy.m_iChannels;
	m_iNextSourceFrame = cpy.m_iNextSourceFrame;
	m_fCurrentStreamToSourceRatio = cpy.m_fCurrentStreamToSourceRatio;

	for (RageSoundReader const* it : cpy.m_aSounds)
		m_aSounds.push_back(it->Copy());
}

void RageSoundReader_Merge::AddSound(RageSoundReader* pSound)
{
	m_aSounds.push_back(pSound);
}

/* If every sound has the same sample rate, return it.  Otherwise, return -1. */
int RageSoundReader_Merge::GetSampleRateInternal() const
{
	int iRate = -1;
	for (RageSoundReader const* it : m_aSounds)
	{
		if (iRate == -1)
			iRate = it->GetSampleRate();
		else if (iRate != it->GetSampleRate())
			return -1;
	}
	return iRate;
}

void RageSoundReader_Merge::Finish(int iPreferredSampleRate)
{
	m_iChannels = 1;
	for (RageSoundReader* it : m_aSounds)
		m_iChannels = std::max(m_iChannels, it->GetNumChannels());

	m_iSampleRate = GetSampleRateInternal();
	if (m_iSampleRate == -1)
	{
		for (RageSoundReader* it : m_aSounds)
		{
			RageSoundReader_Resample_Good* pResample = new RageSoundReader_Resample_Good(it, iPreferredSampleRate);
			it = pResample;
		}

		m_iSampleRate = iPreferredSampleRate;
	}

	for (RageSoundReader* it : m_aSounds)
	{
		if (it->GetNumChannels() != this->GetNumChannels())
			it = new RageSoundReader_Pan(it);
	}

	if (m_iChannels > 2)
	{
		std::vector<RageSoundReader*> aSounds;
		for (RageSoundReader* it : m_aSounds)
		{
			if (it->GetNumChannels() != m_iChannels)
			{
				LOG->Warn("Discarded sound with %i channels, not %i",
					it->GetNumChannels(), m_iChannels);
				delete it;
				it = nullptr;
			}
			else
			{
				aSounds.push_back(it);
			}
		}
		m_aSounds = aSounds;
	}
}

int RageSoundReader_Merge::SetPosition(int iFrame)
{
	m_iNextSourceFrame = iFrame;

	int iRet = 0;
	for (int i = 0; i < (int)m_aSounds.size(); ++i)
	{
		RageSoundReader* pSound = m_aSounds[i];
		int iThisRet = pSound->SetPosition(iFrame);
		if (iThisRet == -1)
			return -1;
		if (iThisRet > 0)
			iRet = 1;
	}

	return iRet;
}

bool RageSoundReader_Merge::SetProperty(const RString& sProperty, float fValue)
{
	bool bRet = false;
	for (unsigned i = 0; i < m_aSounds.size(); ++i)
	{
		if (m_aSounds[i]->SetProperty(sProperty, fValue))
			bRet = true;
	}

	return bRet;
}

static float Difference(float a, float b) { return std::abs(a - b); }
static int Difference(int a, int b) { return std::abs(a - b); }

static const int ERROR_CORRECTION_THRESHOLD = 10;


int RageSoundReader_Merge::Read(float* pBuffer, int iFrames)
{
	if (m_aSounds.empty())
		return END_OF_FILE;

	std::vector<int> aNextSourceFrames(m_aSounds.size());
	std::vector<float> aRatios(m_aSounds.size());

	for (size_t i = 0; i < m_aSounds.size(); ++i)
	{
		aNextSourceFrames[i] = m_aSounds[i]->GetNextSourceFrame();
		aRatios[i] = m_aSounds[i]->GetStreamToSourceRatio();
	}

	int iEarliestSound = std::distance(aNextSourceFrames.begin(), std::min_element(aNextSourceFrames.begin(), aNextSourceFrames.end()));

	if (m_iNextSourceFrame != aNextSourceFrames[iEarliestSound] ||
		m_fCurrentStreamToSourceRatio != aRatios[iEarliestSound])
	{
		m_iNextSourceFrame = aNextSourceFrames[iEarliestSound];
		m_fCurrentStreamToSourceRatio = aRatios[iEarliestSound];
		return 0;
	}

	int iMinPosition = aNextSourceFrames[iEarliestSound];
	for (size_t i = 0; i < m_aSounds.size(); ++i)
	{
		if (Difference(aNextSourceFrames[i], iMinPosition) > ERROR_CORRECTION_THRESHOLD)
		{
			int iMaxSourceFramesToRead = aNextSourceFrames[i] - iMinPosition;
			int iMaxStreamFramesToRead = static_cast<int>((iMaxSourceFramesToRead / m_fCurrentStreamToSourceRatio) + 0.5);
			iFrames = std::min(iFrames, iMaxStreamFramesToRead);
		}
	}

	if (m_aSounds.size() == 1)
	{
		RageSoundReader* pSound = m_aSounds.front();
		iFrames = pSound->Read(pBuffer, iFrames);
		if (iFrames > 0)
			m_iNextSourceFrame += static_cast<int>((iFrames * m_fCurrentStreamToSourceRatio) + 0.5);
		return iFrames;
	}

	std::vector<float> mixBuffer(iFrames * m_iChannels, 0.0f);
	float Buffer[2048];
	iFrames = std::min(iFrames, static_cast<int>(sizeof(Buffer) / sizeof(Buffer[0]) / m_iChannels));

	std::atomic<int> iFramesRead(0);
	std::mutex mixBufferMutex;

	auto mixSound = [&](RageSoundReader* pSound, size_t soundIndex) {
		int localFramesRead = 0;
		while (localFramesRead < iFrames)
		{
			if (Difference(aNextSourceFrames[soundIndex], m_iNextSourceFrame + static_cast<int>((localFramesRead * aRatios[soundIndex]) + 0.5)) > ERROR_CORRECTION_THRESHOLD)
			{
				LOG->Trace("Sound positions moving at different rates");
				break;
			}

			int iGotFrames = pSound->Read(Buffer, iFrames - localFramesRead);
			aNextSourceFrames[soundIndex] = m_aSounds[soundIndex]->GetNextSourceFrame();
			aRatios[soundIndex] = m_aSounds[soundIndex]->GetStreamToSourceRatio();
			if (iGotFrames < 0)
			{
				if (soundIndex == 0)
					return;
				break;
			}

			{
				std::lock_guard<std::mutex> lock(mixBufferMutex);
				for (int j = 0; j < iGotFrames * pSound->GetNumChannels(); ++j)
				{
					mixBuffer[localFramesRead * pSound->GetNumChannels() + j] += Buffer[j];
				}
			}
			localFramesRead += iGotFrames;

			if (Difference(aRatios[soundIndex], m_fCurrentStreamToSourceRatio) > 0.001f)
				break;
		}
		iFramesRead.fetch_add(localFramesRead, std::memory_order_relaxed);
		};

	std::vector<std::thread> threads;
	for (size_t i = 0; i < m_aSounds.size(); ++i)
	{
		threads.emplace_back(mixSound, m_aSounds[i], i);
	}

	for (auto& thread : threads)
	{
		thread.join();
	}

	std::memcpy(pBuffer, mixBuffer.data(), iFrames * m_iChannels * sizeof(float));
	m_iNextSourceFrame += static_cast<int>((iFramesRead.load(std::memory_order_relaxed) * m_fCurrentStreamToSourceRatio) + 0.5);

	return iFramesRead.load(std::memory_order_relaxed);
}


int RageSoundReader_Merge::GetLength() const
{
	int iLength = 0;
	for (unsigned i = 0; i < m_aSounds.size(); ++i)
		iLength = std::max(iLength, m_aSounds[i]->GetLength());
	return iLength;
}

int RageSoundReader_Merge::GetLength_Fast() const
{
	int iLength = 0;
	for (unsigned i = 0; i < m_aSounds.size(); ++i)
		iLength = std::max(iLength, m_aSounds[i]->GetLength_Fast());
	return iLength;
}

/*
 * Copyright (c) 2004-2006 Glenn Maynard
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
