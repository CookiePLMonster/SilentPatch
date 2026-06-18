#include "StdAfxSA.h"
#include "PNGFile.h"

#include "RWUtils.hpp"

bool CPNGFile::HasGameBindings()
{
	using namespace RWBindings;
	return RtPNGImageRead() && RwRasterCreate() && RwTextureCreate() && RwImageDestroy();
}

RwTexture* CPNGFile::ReadFromFile(const char* pFileName)
{
	RwTexture*		pTexture = nullptr;

	if ( RwImage* pImage = RtPNGImageRead(pFileName) )
	{
		RwInt32		dwWidth, dwHeight, dwDepth, dwFlags;
		RwImageFindRasterFormat(pImage, rwRASTERTYPETEXTURE, &dwWidth, &dwHeight, &dwDepth, &dwFlags);
		if ( RwRaster* pRaster = RwRasterCreate(dwWidth, dwHeight, dwDepth, dwFlags) )
		{
			RwRasterSetFromImage(pRaster, pImage);

			pTexture = RwTextureCreate(pRaster);
			//if ( pTexture )
			//	RwTextureSetName(pTexture, pFileName);
		}
		RwImageDestroy(pImage);
	}
	return pTexture;
}

RwTexture* CPNGFile::ReadFromMemory(const void* pMemory, unsigned int nLen)
{
	static uint8_t* const	pMem = AddressByVersion<uint8_t*>(0x7CF9CA, 0x7D02CA, 0x80998A);
	RwTexture*				pTexture = nullptr;

	uint8_t oldMemory = *pMem;
	Memory::VP::Patch<uint8_t>(pMem, rwSTREAMMEMORY);

	RwMemory	PNGMemory;
	PNGMemory.start = const_cast<RwUInt8*>(static_cast<const RwUInt8*>(pMemory));
	PNGMemory.length = nLen;

	if ( RwImage* pImage = RtPNGImageRead(reinterpret_cast<RwChar*>(&PNGMemory)) )
	{
		RwInt32		dwWidth, dwHeight, dwDepth, dwFlags;
		RwImageFindRasterFormat(pImage, rwRASTERTYPETEXTURE, &dwWidth, &dwHeight, &dwDepth, &dwFlags);
		if ( RwRaster* pRaster = RwRasterCreate(dwWidth, dwHeight, dwDepth, dwFlags) )
		{
			RwRasterSetFromImage(pRaster, pImage);

			pTexture = RwTextureCreate(pRaster);
			//if ( pTexture )
			//	RwTextureSetName(pTexture, pFileName);
		}
		RwImageDestroy(pImage);
	}

	Memory::VP::Patch<uint8_t>(pMem, oldMemory);

	return pTexture;
}