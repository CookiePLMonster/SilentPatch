#ifndef __AUDIOHARDWARE
#define __AUDIOHARDWARE

// IStream
#include <objidl.h>

enum eDecoderType
{
	DECODER_NULL,
	DECODER_VORBIS,
	DECODER_WAVE,
	DECODER_WINDOWSMEDIA,
	DECODER_QUICKTIME,

	// Custom
	DECODER_FLAC
};

// 1.0/Steam structure
class NOVMT CAEDataStreamOld final : IStream
{
private:
	HANDLE			hHandle;
	char*			pFilename;
	bool			bOpened;
	DWORD			dwCurrentPosition, dwStartPosition;
	DWORD			dwLength;
	DWORD			dwID;
	bool			bEncrypted;
	LONG			nRefCount;

public:
	void			operator delete(void* data)
	{
		// Call SA operator delete
		GTAdelete(data);
	}

	CAEDataStreamOld() = delete;

	~CAEDataStreamOld()
	{
		if ( bOpened )
		{
			CloseHandle(hHandle);
			bOpened = false;
		}
		if ( pFilename != nullptr )
		{
			GTAdelete(pFilename);
			pFilename = nullptr;
		}
	}

	inline DWORD	GetID() const
		{ return dwID; }
	inline HANDLE	GetFile() const
		{ return hHandle; }

public:
	// Custom methods
	DWORD		Seek(LONG nToSeek, DWORD nPoint);
	DWORD		FillBuffer(void* pBuf, DWORD nLen);
	DWORD		GetCurrentPosition() const
	{
		LARGE_INTEGER filePointer;
		filePointer.QuadPart = 0;
		SetFilePointerEx( hHandle, filePointer, &filePointer, FILE_CURRENT );	
		return DWORD(filePointer.QuadPart - dwStartPosition);
	}
};

// 1.01 structure
class NOVMT CAEDataStreamNew final : IStream
{
private:
	void*			pUselessMalloc;

	HANDLE			hHandle;
	char*			pFilename;
	bool			bOpened;
	DWORD			dwCurrentPosition, dwStartPosition;
	DWORD			dwLength;
	DWORD			dwID;
	bool			bEncrypted;
	LONG			nRefCount;

public:
	void			operator delete(void* data)
	{
		// Call SA operator delete
		GTAdelete(data);
	}

	CAEDataStreamNew() = delete;

	~CAEDataStreamNew()
	{
		if ( bOpened )
		{
			CloseHandle(hHandle);
			bOpened = false;
		}
		if ( pFilename != nullptr )
		{
			GTAdelete(pFilename);
			pFilename = nullptr;
		}
	}

	inline DWORD	GetID() const
		{ return dwID; }
	inline HANDLE	GetFile() const
		{ return hHandle; }

public:
	// Custom methods
	DWORD		Seek(LONG nToSeek, DWORD nPoint);
	DWORD		FillBuffer(void* pBuf, DWORD nLen);
	DWORD		GetCurrentPosition() const
	{
		LARGE_INTEGER filePointer;
		filePointer.QuadPart = 0;
		SetFilePointerEx( hHandle, filePointer, &filePointer, FILE_CURRENT );	
		return DWORD(filePointer.QuadPart - dwStartPosition);
	}
};

class CAEDataStream
{
private:
	static bool			m_bUseNewStruct;

private:
	~CAEDataStream() = delete;
	CAEDataStream() = delete;

public:
	static void			SetStructType(bool bNew)
	{ m_bUseNewStruct = bNew; }
	static bool			IsNew()
		{ return m_bUseNewStruct; }

	// This is handled by GTA so we can leave it that way
	bool				Initialise();

	unsigned int		Seek(long nToSeek, int nPoint)
	{	if ( m_bUseNewStruct ) 
			return reinterpret_cast<CAEDataStreamNew*>(this)->Seek(nToSeek, nPoint);
		return reinterpret_cast<CAEDataStreamOld*>(this)->Seek(nToSeek, nPoint); }

	unsigned int		FillBuffer(void* pBuf, unsigned long nLen)
	{	if ( m_bUseNewStruct ) 
			return reinterpret_cast<CAEDataStreamNew*>(this)->FillBuffer(pBuf, nLen);
		return reinterpret_cast<CAEDataStreamOld*>(this)->FillBuffer(pBuf, nLen); }

	unsigned int		GetCurrentPosition() const
	{	if ( m_bUseNewStruct ) 
			return reinterpret_cast<const CAEDataStreamNew*>(this)->GetCurrentPosition();
		return reinterpret_cast<const CAEDataStreamOld*>(this)->GetCurrentPosition(); }

	inline DWORD	GetID() const
	{	if ( m_bUseNewStruct ) 
			return reinterpret_cast<const CAEDataStreamNew*>(this)->GetID();
		return reinterpret_cast<const CAEDataStreamOld*>(this)->GetID(); }

	inline HANDLE	GetFile() const
	{	if ( m_bUseNewStruct ) 
			return reinterpret_cast<const CAEDataStreamNew*>(this)->GetFile();
		return reinterpret_cast<const CAEDataStreamOld*>(this)->GetFile(); }
};


class CAEStreamingDecoder
{
private:
	CAEDataStream*		pStream;

public:
	CAEStreamingDecoder(CAEDataStream* stream)
		: pStream(stream)
	{
		if ( stream != nullptr )
			stream->Initialise();
	}

	inline CAEDataStream*	GetStream()
		{ return pStream; }
	inline const CAEDataStream*	GetStream() const
	{ return pStream; }

	virtual bool			Initialise()=0;
	virtual uint32_t		FillBuffer(void* pBuf,uint32_t nLen)=0;

	virtual uint32_t		GetStreamLengthMs() const =0;
	virtual uint32_t		GetStreamPlayTimeMs() const =0;
	virtual void			SetCursor(uint32_t nTime)=0;
	virtual uint32_t		GetSampleRate() const =0;

	virtual					~CAEStreamingDecoder();

	virtual uint32_t		GetStreamID() const =0;
};

static_assert(sizeof(CAEDataStreamOld) == 0x28, "Wrong size: CAEDataStreamOld");
static_assert(sizeof(CAEDataStreamNew) == 0x2C, "Wrong size: CAEDataStreamNew");

#endif
