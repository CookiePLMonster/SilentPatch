#define WIN32_LEAN_AND_MEAN

#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define NOMINMAX

#include <windows.h>
#include <cassert>
#include <cstdio>

#include "Utils/MemoryMgr.h"
#include "MemoryMgr.GTA.h"
#include "Utils/Patterns.h"

struct RwObjectNameIdAssocation
{
	const char*	pName;
	int32_t		hierId;
	uint32_t	flags;
};

// Fix for conformance-
typedef struct IUnknown IUnknown;
