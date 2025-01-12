#include "StdAfxSA.h"
#define DR_FLAC_IMPLEMENTATION
#include "FLACDecoderSA.h"

bool CAEFLACDecoder::Initialise()
{
	DWORD size;
	BOOL result = ReadFile(GetStream()->GetFile(), m_buffer, sizeof(m_buffer), &size, nullptr);

	if ( result == FALSE || size == 0 )
		return false;

	m_FLACdecoder = drflac_open_memory(m_buffer, size, nullptr);
	assert(m_FLACdecoder != nullptr);
	if ( m_FLACdecoder == nullptr )
		return false;

	m_eof = false;
	return m_FLACdecoder->sampleRate <= 48000 && (m_FLACdecoder->bitsPerSample == 8 || m_FLACdecoder->bitsPerSample == 16 || m_FLACdecoder->bitsPerSample == 24);
}

uint32_t CAEFLACDecoder::FillBuffer(void* pBuf, uint32_t nLen)
{
	uint32_t		samplesToDecode = nLen / (2 * sizeof(int16_t));
	uint32_t		bytesDecoded = 0;
	int16_t*		outputBuffer = static_cast<int16_t*>(pBuf);
	drflac_int32*	inputBuffer[] = { m_buffer+m_bufferCursor, m_buffer+m_bufferCursor+m_curBlockSize };

	const uint32_t	sampleWidth = m_FLACdecoder->bitsPerSample;
	const bool		stereo = m_FLACdecoder->channels > 1;

	while ( bytesDecoded < nLen )
	{
		if ( m_bufferCursor >= m_curBlockSize )
		{
			// New FLAC frame needed
			if ( m_FLACdecoder->memoryStream.currentReadPos == m_FLACdecoder->memoryStream.dataSize )
				break;

			drflac_seek_to_pcm_frame(m_FLACdecoder, (drflac_int64)inputBuffer);
			inputBuffer[0] = m_buffer;
			inputBuffer[1] = m_buffer+m_curBlockSize;
		}

		size_t samplesThisIteration = std::min( m_curBlockSize-m_bufferCursor, samplesToDecode );
		if ( sampleWidth == 8 )
		{
			if ( stereo )
			{
				for ( size_t i = 0; i < samplesThisIteration; ++i )
				{
					outputBuffer[0] = int16_t(*inputBuffer[0]++ << 8);
					outputBuffer[1] = int16_t(*inputBuffer[1]++ << 8);
					outputBuffer += 2;
				}
			}
			else
			{
				for ( size_t i = 0; i < samplesThisIteration; ++i )
				{
					outputBuffer[0] = outputBuffer[1] = int16_t(*inputBuffer[0]++ << 8);
					outputBuffer += 2;
				}
			}
		}
		else if ( sampleWidth == 24 )
		{
			// 24-bit
			if ( stereo )
			{
				for ( size_t i = 0; i < samplesThisIteration; ++i )
				{
					outputBuffer[0] = int16_t(*inputBuffer[0]++ >> 8);
					outputBuffer[1] = int16_t(*inputBuffer[1]++ >> 8);
					outputBuffer += 2;
				}
			}
			else
			{
				for ( size_t i = 0; i < samplesThisIteration; ++i )
				{
					outputBuffer[0] = outputBuffer[1] = int16_t(*inputBuffer[0]++ >> 8);
					outputBuffer += 2;
				}
			}
		}
		else
		{
			if ( stereo )
			{
				for ( size_t i = 0; i < samplesThisIteration; ++i )
				{
					outputBuffer[0] = int16_t(*inputBuffer[0]++);
					outputBuffer[1] = int16_t(*inputBuffer[1]++);
					outputBuffer += 2;
				}
			}
			else
			{
				for ( size_t i = 0; i < samplesThisIteration; ++i )
				{
					outputBuffer[0] = outputBuffer[1] = int16_t(*inputBuffer[0]++);
					outputBuffer += 2;
				}
			}
		}

		m_currentSample += samplesThisIteration;
		m_bufferCursor += samplesThisIteration;
		samplesToDecode -= samplesThisIteration;
		bytesDecoded += samplesThisIteration * 2 * sizeof(int16_t);
	}
	return bytesDecoded;
}

CAEFLACDecoder::~CAEFLACDecoder()
{
	if ( m_FLACdecoder != nullptr ) drflac_close(m_FLACdecoder);
	delete[] m_buffer;
}