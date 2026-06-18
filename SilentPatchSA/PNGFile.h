#ifndef __PNGFILE
#define __PNGFILE

class CPNGFile
{
public:
	static RwTexture*			ReadFromFile(const char* pFileName);
	static RwTexture*			ReadFromMemory(const void* pMemory, unsigned int nLen);

	static bool					HasGameBindings();
};

#endif