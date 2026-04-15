#pragma once

#include <cstdint>
#include <cstddef>

#include "Maths.h"
#include "TheFLAUtils.h"

enum // m_objectCreatedBy
{
	GAME_OBJECT = 1,
	MISSION_OBJECT = 2,
	TEMP_OBJECT = 3,
};

class CEntity
{
public:
	void*		__vmt;
	CMatrix		m_matrix;
	void*		clump;
	uint8_t		m_nType : 3;
	std::byte	__pad4[11];
	FLAUtils::int16 m_modelIndex;

public:
	int32_t GetModelIndex() const
		{ return m_modelIndex.Get(); }

	const CMatrix& GetMatrix() const
		{ return m_matrix; }
};
