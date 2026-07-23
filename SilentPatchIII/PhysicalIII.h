#pragma once

#include <cstdint>
#include "Maths.h"
#include "TheFLAUtils.h"
#include "ExternalBindings.hpp"

class CPlaceable
{
protected:
	void*		__vmt;
	CMatrix		m_matrix;

public:
	const CMatrix& GetMatrix() const
	{
		return m_matrix;
	}

	inline const CVector& GetPosition() const
	{
		return m_matrix.GetTranslate();
	}
};

static_assert(sizeof(CPlaceable) == 0x4C, "Wrong size: CPlaceable");

class CEntity : public CPlaceable
{
protected:
	uint8_t		__pad2[16];
	FLAUtils::int16 m_modelIndex;

	int16_t		m_level;
	class CReference* pReferences;

public:
	int32_t GetModelIndex() const
	{ return m_modelIndex.Get(); }
};

static_assert(sizeof(CEntity) == 0x64, "Wrong size: CEntity");

class CPhysical : public CEntity
{
public:
	int32_t m_audioEntityId;

	float _unused;
	class CTreadable *m_treadable[2];
	uint32_t m_nLastTimeCollided;

	CVector m_vecMoveSpeed;
	CVector m_vecTurnSpeed;

	CVector m_vecMoveFriction;
	CVector m_vecTurnFriction;

	CVector m_vecAverageMoveSpeed;
	CVector m_vecAverageTurnSpeed;

	float m_fMass;
	float m_fTurnMass;

	std::byte __pad_physical[96];

public:
	static ExternalMethod<CPhysical, void(float fX, float fY, float fZ)> ApplyMoveForce;
};

static_assert(sizeof(CPhysical) == 0x128, "Wrong size: CPhysical");
