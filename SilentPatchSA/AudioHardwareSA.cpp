#include "StdAfxSA.h"
#include "AudioHardwareSA.h"

bool			CAEDataStream::m_bUseNewStruct;

ExternalMethod<CAEDataStream, bool()> CAEDataStream::Initialise(AddressByVersion<bool (__thiscall*)(CAEDataStream*)>(0x4DC2B0, 0x4DC7A0, 0x4E7550));

// Work around libflac linking issues with older MSVC
#if _MSC_VER < 1930

extern "C" unsigned long __declspec(naked)  _dtoul3_legacy(const double x) {
	_asm
	{
		cvtsd2si	eax, xmm0
		xor			edx, edx
		retn
	}
}

extern "C" __declspec(naked) float _ltof3(long x) {
	_asm
	{
		cvtsi2ss	xmm0, ecx
		retn
	}
}

#endif

DWORD CAEDataStreamOld::Seek(LONG nToSeek, DWORD nPoint)
{
	LARGE_INTEGER filePosition;
	switch ( nPoint )
	{
	case FILE_BEGIN:
		filePosition.QuadPart = nToSeek + dwStartPosition;
		break;
	case FILE_END:
		nPoint = FILE_BEGIN;
		filePosition.QuadPart = dwStartPosition + dwLength - nToSeek;
		break;
	default:
		filePosition.QuadPart = nToSeek;
		break;
	}

	SetFilePointerEx(hHandle, filePosition, &filePosition, nPoint);
	dwCurrentPosition = filePosition.LowPart;

	return dwCurrentPosition - dwStartPosition;
}

DWORD CAEDataStreamOld::FillBuffer(void* pBuf, DWORD nLen)
{
	ReadFile(hHandle, pBuf, nLen, &nLen, nullptr);
	dwCurrentPosition += nLen;
	return nLen;
}

DWORD CAEDataStreamNew::Seek(LONG nToSeek, DWORD nPoint)
{
	LARGE_INTEGER filePosition;
	switch ( nPoint )
	{
	case FILE_BEGIN:
		filePosition.QuadPart = nToSeek + dwStartPosition;
		break;
	case FILE_END:
		nPoint = FILE_BEGIN;
		filePosition.QuadPart = dwStartPosition + dwLength - nToSeek;
		break;
	default:
		filePosition.QuadPart = nToSeek;
		break;
	}

	SetFilePointerEx(hHandle, filePosition, &filePosition, nPoint);
	dwCurrentPosition = filePosition.LowPart;

	return dwCurrentPosition - dwStartPosition;
}

DWORD CAEDataStreamNew::FillBuffer(void* pBuf, DWORD nLen)
{
	ReadFile(hHandle, pBuf, nLen, &nLen, nullptr);
	dwCurrentPosition += nLen;
	return nLen;
}

CAEStreamingDecoder::~CAEStreamingDecoder()
{
	if ( CAEDataStream::IsNew() )
		delete reinterpret_cast<CAEDataStreamNew*>(pStream);
	else
		delete reinterpret_cast<CAEDataStreamOld*>(pStream);
}