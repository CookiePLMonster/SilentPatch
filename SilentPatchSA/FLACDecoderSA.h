#pragma once

#include "AudioHardwareSA.h"

#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_OGG
#include "dr_flac.h"

class CAEFLACDecoder final : public CAEStreamingDecoder
{
private:
	drflac*				m_FLACdecoder = nullptr;
	drflac_uint64		m_currentSample;
	drflac_int32*		m_buffer = nullptr;
	size_t				m_curBlockSize, m_maxBlockSize = 0;
	size_t				m_bufferCursor;
	bool				m_eof;

public:
	CAEFLACDecoder(CAEDataStream* stream)
		: CAEStreamingDecoder(stream)
	{}

	virtual					~CAEFLACDecoder() override;
	virtual bool			Initialise() override;
	virtual uint32_t		FillBuffer(void* pBuf, uint32_t nLen) override;
	virtual uint32_t		GetStreamLengthMs() const override
	{ return uint32_t((m_FLACdecoder->totalPCMFrameCount * 1000) / m_FLACdecoder->sampleRate); }
	virtual uint32_t		GetStreamPlayTimeMs() const override
	{ return uint32_t((m_currentSample * 1000) / m_FLACdecoder->sampleRate); }
	virtual void			SetCursor(uint32_t nTime) override
	{ drflac_seek_to_pcm_frame(m_FLACdecoder, (uint64_t(nTime) * m_FLACdecoder->sampleRate) / 1000); }
	virtual uint32_t		GetSampleRate() const override
	{ return m_FLACdecoder->sampleRate; }
	virtual uint32_t		GetStreamID() const override
	{ return GetStream()->GetID(); }
};