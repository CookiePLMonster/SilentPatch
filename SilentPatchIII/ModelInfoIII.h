#pragma once

#include <cstdint>
#include <functional>
#include "Maths.h"

#include <rwcore.h>
#include <rpworld.h>

// This really belongs in Maths.h but San Andreas optimized those structured heavily...
struct CColSphere
{
	CVector m_center;
	float m_radius;
	uint8_t m_surface;
	uint8_t m_piece;

	void Set(float radius, const CVector& center, uint8_t surf, uint8_t piece)
	{
		m_center = center;
		m_radius = radius;
		m_surface = surf;
		m_piece = piece;
	}

	void Set(float radius, const CVector& center)
	{
		m_center = center;
		m_radius = radius;
	}

	constexpr CColSphere( float radius, const CVector& center, uint8_t surf, uint8_t piece )
		: m_center( center ), m_radius( radius ), m_surface( surf ), m_piece( piece )
	{
	}
};

struct CColBox
{
	CVector m_min;
	CVector m_max;
	uint8_t m_surface;
	uint8_t m_piece;

	void Set(const CVector& min, const CVector& max, uint8_t surf, uint8_t piece)
	{
		m_min = min;
		m_max = max;
		m_surface = surf;
		m_piece = piece;
	}

	CVector GetSize() const { return m_max - m_min; }

	constexpr CColBox( const CVector& min, const CVector& max, uint8_t surf, uint8_t piece )
		: m_min( min ), m_max( max ), m_surface( surf ), m_piece( piece )
	{
	}
};

struct CColModel
{
	CColSphere m_boundingSphere;
	CColBox m_boundingBox;
	short m_m_numSpheres = 0;
	short m_numLines = 0;
	short m_numBoxes = 0;
	short m_numTriangles = 0;
	int m_level = 0;
	bool m_ownsCollisionVolumes = false;
	CColSphere* m_spheres = nullptr;
	struct CColLine* m_lines = nullptr;
	CColBox* m_boxes = nullptr;
	CVector* m_vertices = nullptr;
	struct CColTriangle* m_triangles = nullptr;
	struct CColTrianglePlane* m_trianglePlanes = nullptr;

	constexpr CColModel( const CColSphere& sphere, const CColBox& box )
		: m_boundingSphere( sphere ), m_boundingBox ( box )
	{
	}
};

class CSimpleModelInfo
{
private:
	void*	__vmt;
	char	m_name[24];
	uint8_t __pad[32];
	float	m_lodDistances[3];
	uint8_t	__pad2[4];

public:
	void	SetNearDistanceForLOD_SilentPatch();
};

static_assert(sizeof(CSimpleModelInfo) == 0x4C, "Wrong size: CSimpleModelInfo");

class CVehicleModelInfo
{
private:
	std::byte	__pad[472];
	RwTexture*	m_envMap;
	RpAtomic*	m_comps[6];
	int32_t		m_numComps;

public:
	static RpAtomic* (*SetEnvironmentMapCB)(RpAtomic* atomic, void* data);

	static inline void (CVehicleModelInfo::*orgSetEnvironmentMap)();
	void SetEnvironmentMap_ExtraComps();

	// For SkyGfx interop
	static void AttachCarPipeToRwObject_Default(RwObject*) { }
	static inline void (*AttachCarPipeToRwObject)(RwObject* object) = &AttachCarPipeToRwObject_Default;
};
static_assert(sizeof(CVehicleModelInfo) == 0x1F8, "Wrong size: CVehicleModelInfo");
