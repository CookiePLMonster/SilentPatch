#pragma once

#define RwEngineInstance (*rwengine)

extern void** rwengine;

namespace RWGTA::Patches
{
	bool TryLocateRwD3D8();
}
