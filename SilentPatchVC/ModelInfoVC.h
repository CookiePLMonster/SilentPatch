#pragma once

#include <rwcore.h>
#include <rpworld.h>

#define MAX_MODEL_NAME (24)

class CVehicleModelInfo
{
public:
	void*		 __vmt;
	char		 m_name[MAX_MODEL_NAME];
	uint8_t		 __pad1[12];
	RpClump*	 m_clump;
	uint8_t		 __pad3[16];
	unsigned int m_dwType;
	uint8_t		 __pad4[11];
	int8_t		 m_numComps;
	uint8_t		 __pad2[268];
	RpAtomic*	 m_comps[6];
	uint8_t		 __pad5[4];

public:
	RwFrame*		GetExtrasFrame( RpClump* clump );
	const char*		GetModelName() const { return m_name; }

	static bool		HasGameBindings_Extras();

	// For SkyGfx interop
	static void AttachCarPipeToRwObject_Default(RwObject*) { }
	static inline void (*AttachCarPipeToRwObject)(RwObject* object) = &AttachCarPipeToRwObject_Default;
};
static_assert(sizeof(CVehicleModelInfo) == 0x174, "Wrong size: CVehicleModelInfo");

class CSimpleModelInfo
{
public:
	void*	__vmt;
	char	m_name[24];
	uint8_t __pad[12];
	void*	m_atomics[3];
	float	m_lodDistances[3];
	uint8_t	__pad2[4];
};
static_assert(sizeof(CSimpleModelInfo) == 68, "Wrong size: CSimpleModelInfo");
