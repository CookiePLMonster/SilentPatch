#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

#define RwEngineInstance (*rwengine)

#include <rwcore.h>

// GTA versions of RenderWare functions/macros for GTA III/Vice City
// since we cannot use RwEngineInstance directly

// Anything originally using RwEngineInstance shall be redefined here
// Functions which RW 3.6 inlined can also easily be defined here

void** rwengine = []() -> void** {
	// Thanks Steam III...

	// Locate RwRenderStateSet
	try
	{
		auto renderStateSetPtr = hook::txn::get_pattern( "D1 7C 24 2C", 4 );
		auto renderStateSet = reinterpret_cast<uintptr_t>(Memory::ReadCallFrom( renderStateSetPtr ));

		// Test III 1.0/1.1/VC
		if ( *reinterpret_cast<uint8_t*>(renderStateSet) == 0xA1 )
		{
			return *reinterpret_cast<void***>(renderStateSet + 1);
		}

		// Test III Steam
		renderStateSet += 3;
		if ( *reinterpret_cast<uint8_t*>(renderStateSet) == 0xA1 )
		{
			return *reinterpret_cast<void***>(renderStateSet + 1);
		}
	}
	TXN_CATCH();

	assert(!"Could not locate RwEngineInstance!");
	return nullptr;
}();

static void* varRwD3D8SetRenderState = Memory::ReadCallFrom( hook::get_pattern( "0F 8C ? ? ? ? 6A 05 6A 19", 10 ) );
WRAPPER RwBool RwD3D8SetRenderState(RwUInt32 state, RwUInt32 value) { VARJMP(varRwD3D8SetRenderState); }

static RwUInt32* _rwD3D8RenderStates = *static_cast<RwUInt32**>(Memory::ReadCallFrom( hook::get_pattern( "0F 8C ? ? ? ? 6A 05 6A 19", 10 ), 8 + 3 ));
void RwD3D8GetRenderState(RwUInt32 state, void* value)
{
	RwUInt32* valuePtr = static_cast<RwUInt32*>(value);
	*valuePtr = _rwD3D8RenderStates[ 2 * state ];
}

RwReal RwIm2DGetNearScreenZ()
{
	return RWSRCGLOBAL(dOpenDevice).zBufferNear;
}

RwBool RwRenderStateGet(RwRenderState state, void *value)
{
	return RWSRCGLOBAL(dOpenDevice).fpRenderStateGet(state, value);
}

RwBool RwRenderStateSet(RwRenderState state, void *value)
{
	return RWSRCGLOBAL(dOpenDevice).fpRenderStateSet(state, value);
}

// Unreachable stub
RwBool RwMatrixDestroy(RwMatrix* mpMat) { assert(!"Unreachable!"); return TRUE; }
