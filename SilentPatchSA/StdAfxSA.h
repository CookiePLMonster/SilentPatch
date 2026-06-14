#pragma warning(disable:4458) // declaration hides class member
#pragma warning(disable:4201) // nonatandard extension user: nameless struct/union
#pragma warning(disable:4100) // unreferenced formal parameter

#define WIN32_LEAN_AND_MEAN

#define NOMINMAX
#define WINVER 0x0502
#define _WIN32_WINNT 0x0502

#include <windows.h>
#include <cassert>

#define RwEngineInstance (*rwengine)

#include <rwcore.h>
#include <rphanim.h>
#include <rtpng.h>

#include "Utils/MemoryMgr.h"
#include "MemoryMgr.GTA.h"
#include "Maths.h"
#include "rwutils.hpp"

#include "TheFLAUtils.h"

extern const char* (*GetFrameNodeName)(RwFrame*);
RwObject* GetFirstObject(RwFrame* pFrame);

#ifdef _DEBUG
#define MEM_VALIDATORS 1
#else
#define MEM_VALIDATORS 0
#endif
