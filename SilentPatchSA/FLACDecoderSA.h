#pragma once

#include "AudioHardwareSA.h"

// libflac
#include "FLAC/stream_decoder.h"
#include "FLAC/metadata.h"

class CAEFLACDecoder final : public CAEStreamingDecoder
{
private:
	FLAC__StreamDecoder*		m_FLACdecoder = nullptr;
	FLAC__StreamMetadata*		m_streamMeta = nullptr;
	FLAC__uint64				m_currentSample;
	FLAC__int32*				m_buffer = nullptr;
	size_t						m_curBlockSize, m_maxBlockSize = 0;
	size_t						m_bufferCursor;
	bool						m_eof;

private:
	static FLAC__StreamDecoderReadStatus	read_cb(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data);
	static FLAC__StreamDecoderWriteStatus	write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);
	static void								meta_cb(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata *metadata, void *client_data);
	static void								error_cb(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data);
	static FLAC__StreamDecoderSeekStatus	seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
	static FLAC__StreamDecoderTellStatus	tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
	static FLAC__StreamDecoderLengthStatus	length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
	static FLAC__bool						eof_cb(const FLAC__StreamDecoder *decoder, void *client_data);

public:
	CAEFLACDecoder(CAEDataStream* stream)
		: CAEStreamingDecoder(stream)
	{}

	virtual					~CAEFLACDecoder() override;
	virtual bool			Initialise() override;
	virtual uint32_t		FillBuffer(void* pBuf, uint32_t nLen) override;
	virtual uint32_t		GetStreamLengthMs() const override
	{ return uint32_t((m_streamMeta->data.stream_info.total_samples * 1000) / m_streamMeta->data.stream_info.sample_rate); }
	virtual uint32_t		GetStreamPlayTimeMs() const override
	{ return uint32_t((m_currentSample * 1000) / m_streamMeta->data.stream_info.sample_rate); }
	virtual void			SetCursor(uint32_t nTime) override
	{ FLAC__stream_decoder_seek_absolute(m_FLACdecoder, (uint64_t(nTime) * m_streamMeta->data.stream_info.sample_rate) / 1000); }
	virtual uint32_t		GetSampleRate() const override
	{ return m_streamMeta->data.stream_info.sample_rate; }
	virtual uint32_t		GetStreamID() const override
	{ return GetStream()->GetID(); }
};
