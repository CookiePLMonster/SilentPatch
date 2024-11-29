#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

#include <rwcore.h>

// FIXME: Common.h might be a better place for this (but SA doesn't include it)
#ifdef _MSC_VER
#define NOBUFFERCHECKS __declspec(safebuffers)
#elif defined(__GNUC__) && !defined(__clang__)
#define NOBUFFERCHECKS __attribute__((optimize("-fno-stack-protector")))
#else
#define NOBUFFERCHECKS __attribute__((no_stack_protector))
#endif

constexpr double RAD_TO_DEG (180.0/M_PI);
constexpr double DEG_TO_RAD (M_PI/180.0);

class CRGBA
{
public:
	uint8_t r, g, b, a;

	inline CRGBA() {}

	inline constexpr CRGBA(const CRGBA& in)
		: r(in.r), g(in.g), b(in.b), a(in.a)
	{}

	inline constexpr CRGBA(const CRGBA& in, uint8_t alpha)
		: r(in.r), g(in.g), b(in.b), a(alpha)
	{}


	inline constexpr CRGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
		: r(red), g(green), b(blue), a(alpha)
	{}

	friend constexpr CRGBA Blend(const CRGBA& From, const CRGBA& To, double BlendVal)
		{	const double InvBlendVal = 1.0 - BlendVal;
			return CRGBA( uint8_t(To.r * BlendVal + From.r * InvBlendVal),
			uint8_t(To.g * BlendVal + From.g * InvBlendVal),
			uint8_t(To.b * BlendVal + From.b * InvBlendVal),
			uint8_t(To.a * BlendVal + From.a * InvBlendVal)); }

	friend constexpr CRGBA BlendSqr(const CRGBA& From, const CRGBA& To, double BlendVal)
		{	const double InvBlendVal = 1.0 - BlendVal;
			return CRGBA( uint8_t(sqrt((To.r * To.r) * BlendVal + (From.r * From.r) * InvBlendVal)),
			uint8_t(sqrt((To.g * To.g) * BlendVal + (From.g * From.g) * InvBlendVal)),
			uint8_t(sqrt((To.b * To.b) * BlendVal + (From.b * From.b) * InvBlendVal)),
			uint8_t(sqrt((To.a * To.a) * BlendVal + (From.a * From.a) * InvBlendVal))); }
};

class CRect
{
public:
	float x1, y1;
	float x2, y2;

	inline CRect() {}
	inline constexpr CRect(float a, float b, float c, float d)
		: x1(a), y1(b), x2(c), y2(d)
	{}
};

class CVector
{
public:
	float	x, y, z;

	CVector()
	{}

	constexpr CVector(float fX, float fY, float fZ=0.0f)
		: x(fX), y(fY), z(fZ)
	{}

	constexpr CVector(const RwV3d& rwVec)
		: x(rwVec.x), y(rwVec.y), z(rwVec.z)
	{}

	CVector&		operator+=(const CVector& vec)
			{ x += vec.x; y += vec.y; z += vec.z;
			return *this; }
	CVector&		operator+=(const RwV3d& vec)
			{ x += vec.x; y += vec.y; z += vec.z;
			return *this; }
	CVector&		operator-=(const CVector& vec)
			{ x -= vec.x; y -= vec.y; z -= vec.z;
			return *this; }
	CVector&		operator-=(const RwV3d& vec)
			{ x -= vec.x; y -= vec.y; z -= vec.z;
			return *this; }

	inline float	Magnitude() const
		{ return sqrt(x * x + y * y + z * z); }
	inline constexpr float	MagnitudeSqr() const
		{ return x * x + y * y + z * z; }
	inline CVector&	Normalize()
		{ float	fInvLen = 1.0f / Magnitude(); x *= fInvLen; y *= fInvLen; z *= fInvLen; return *this; }

	friend inline float DotProduct(const CVector& vec1, const CVector& vec2)
		{ return vec1.x * vec2.x + vec1.x * vec2.y + vec1.z * vec2.z; }
	friend inline CVector CrossProduct(const CVector& vec1, const CVector& vec2)
		{ return CVector(	vec1.y * vec2.z - vec1.z * vec2.y,
							vec1.z * vec2.x - vec1.x * vec2.z,
							vec1.x * vec2.y - vec1.y * vec2.x); }

	friend inline CVector operator*(const CVector& in, float fMul)
		{ return CVector(in.x * fMul, in.y * fMul, in.z * fMul); }
	friend inline CVector operator+(const CVector& vec1, const CVector& vec2)
		{ return CVector(vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z); }
	friend inline CVector operator+(const CVector& vec1, const RwV3d& vec2)
		{ return CVector(vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z); }
	friend inline CVector operator-(const CVector& vec1, const CVector& vec2)
		{ return CVector(vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z); }
	friend inline CVector operator-(const CVector& vec1, const RwV3d& vec2)
		{ return CVector(vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z); }
	friend inline CVector operator-(const CVector& vec)
		{ return CVector(-vec.x, -vec.y, -vec.z); }

	inline CVector& FromMultiply(const class CMatrix& mat, const CVector& vec);
	inline CVector& FromMultiply3X3(const class CMatrix& mat, const CVector& vec);
};

class CVector2D
{
public:
	float	x, y;

	CVector2D()
	{}

	constexpr CVector2D(float fX, float fY)
		: x(fX), y(fY)
	{}

	CVector2D&		operator+=(const CVector2D& vec)
			{ x += vec.x; y += vec.y;
			return *this; }
	CVector2D&		operator-=(const CVector2D& vec)
			{ x -= vec.x; y -= vec.y;
			return *this; }

	inline float	Magnitude() const
		{ return sqrt(x * x + y * y); }
	inline constexpr float	MagnitudeSqr() const
		{ return x * x + y * y; }
	inline CVector2D&	Normalize()
		{ float	fInvLen = 1.0f / Magnitude(); x *= fInvLen; y *= fInvLen; return *this; }

	friend inline float DotProduct(const CVector2D& vec1, const CVector2D& vec2)
		{ return vec1.x * vec2.x + vec1.x * vec2.y; }

	friend inline CVector2D operator*(const CVector2D& in, float fMul)
		{ return CVector2D(in.x * fMul, in.y * fMul); }
	friend inline CVector2D operator+(const CVector2D& vec1, const CVector2D& vec2)
		{ return CVector2D(vec1.x + vec2.x, vec1.y + vec2.y); }
	friend inline CVector2D operator-(const CVector2D& vec1, const CVector2D& vec2)
		{ return CVector2D(vec1.x - vec2.x, vec1.y - vec2.y); }
	friend inline CVector2D operator-(const CVector2D& vec)
		{ return CVector2D(-vec.x, -vec.y); }
};

class CSphere
{
public:
	RwSphere	sphere;

public:
	void		Set(float fRadius, const CVector& vecCenter)
	{
		sphere.center.x = vecCenter.x;
		sphere.center.y = vecCenter.y;
		sphere.center.z = vecCenter.z;
		sphere.radius = fRadius;
	}
};

class CMatrix
{
private:
	RwMatrix	m_matrix;
	RwMatrix*	m_pMatrix = nullptr;
	RwBool		m_haveRwMatrix = FALSE;

public:
	inline CMatrix() = default;

	inline CMatrix(RwMatrix* pMatrix, bool bHasMatrix=false)
	{ Attach(pMatrix, bHasMatrix); }

	inline CMatrix(const CMatrix& theMatrix)
		: m_matrix(theMatrix.m_matrix)
	{}

	inline CMatrix(const CVector& vecRight, const CVector& vecUp, const CVector& vecAt, const CVector& vecPos)
	{
		GetRight() = vecRight;
		GetUp() = vecUp;
		GetAt() = vecAt;
		GetPos() = vecPos;
	}

	inline ~CMatrix()
		{	if ( m_haveRwMatrix && m_pMatrix ) 
				RwMatrixDestroy(m_pMatrix); }

	inline CMatrix& operator*=(const CMatrix& right)
	{
		CVector vright(this->m_matrix.right.x * right.m_matrix.right.x + this->m_matrix.right.y * right.m_matrix.up.x + this->m_matrix.right.z * right.m_matrix.at.x + right.m_matrix.pos.x,
			this->m_matrix.right.x * right.m_matrix.right.y + this->m_matrix.right.y * right.m_matrix.up.y + this->m_matrix.right.z * right.m_matrix.at.y + right.m_matrix.pos.y,
			this->m_matrix.right.x * right.m_matrix.right.z + this->m_matrix.right.y * right.m_matrix.up.z + this->m_matrix.right.z * right.m_matrix.at.z + right.m_matrix.pos.z);
		CVector vup(this->m_matrix.up.x * right.m_matrix.right.x + this->m_matrix.up.y * right.m_matrix.up.x + this->m_matrix.up.z * right.m_matrix.at.x + right.m_matrix.pos.x,
			this->m_matrix.up.x * right.m_matrix.right.y + this->m_matrix.up.y * right.m_matrix.up.y + this->m_matrix.up.z * right.m_matrix.at.y + right.m_matrix.pos.y,
			this->m_matrix.up.x * right.m_matrix.right.z + this->m_matrix.up.y * right.m_matrix.up.z + this->m_matrix.up.z * right.m_matrix.at.z + right.m_matrix.pos.z);
		CVector vat(this->m_matrix.at.x * right.m_matrix.right.x + this->m_matrix.at.y * right.m_matrix.up.x + this->m_matrix.at.z * right.m_matrix.at.x + right.m_matrix.pos.x,
			this->m_matrix.at.x * right.m_matrix.right.y + this->m_matrix.at.y * right.m_matrix.up.y + this->m_matrix.at.z * right.m_matrix.at.y + right.m_matrix.pos.y,
			this->m_matrix.at.x * right.m_matrix.right.z + this->m_matrix.at.y * right.m_matrix.up.z + this->m_matrix.at.z * right.m_matrix.at.z + right.m_matrix.pos.z);
		CVector vpos(this->m_matrix.pos.x * right.m_matrix.right.x + this->m_matrix.pos.y * right.m_matrix.up.x + this->m_matrix.pos.z * right.m_matrix.at.x + right.m_matrix.pos.x,
			this->m_matrix.pos.x * right.m_matrix.right.y + this->m_matrix.pos.y * right.m_matrix.up.y + this->m_matrix.pos.z * right.m_matrix.at.y + right.m_matrix.pos.y,
			this->m_matrix.pos.x * right.m_matrix.right.z + this->m_matrix.pos.y * right.m_matrix.up.z + this->m_matrix.pos.z * right.m_matrix.at.z + right.m_matrix.pos.z);

		GetRight() = vright;
		GetUp() = vup;
		GetAt() = vat;
		GetPos() = vpos;

		return *this;
	}

	friend inline CMatrix operator*(const CMatrix& Rot1, const CMatrix& Rot2)
		{ return CMatrix(	CVector(Rot1.m_matrix.right.x * Rot2.m_matrix.right.x + Rot1.m_matrix.right.y * Rot2.m_matrix.up.x + Rot1.m_matrix.right.z * Rot2.m_matrix.at.x + Rot2.m_matrix.pos.x,
								Rot1.m_matrix.right.x * Rot2.m_matrix.right.y + Rot1.m_matrix.right.y * Rot2.m_matrix.up.y + Rot1.m_matrix.right.z * Rot2.m_matrix.at.y + Rot2.m_matrix.pos.y,
								Rot1.m_matrix.right.x * Rot2.m_matrix.right.z + Rot1.m_matrix.right.y * Rot2.m_matrix.up.z + Rot1.m_matrix.right.z * Rot2.m_matrix.at.z + Rot2.m_matrix.pos.z),
						CVector(Rot1.m_matrix.up.x * Rot2.m_matrix.right.x + Rot1.m_matrix.up.y * Rot2.m_matrix.up.x + Rot1.m_matrix.up.z * Rot2.m_matrix.at.x + Rot2.m_matrix.pos.x,
								Rot1.m_matrix.up.x * Rot2.m_matrix.right.y + Rot1.m_matrix.up.y * Rot2.m_matrix.up.y + Rot1.m_matrix.up.z * Rot2.m_matrix.at.y + Rot2.m_matrix.pos.y,
								Rot1.m_matrix.up.x * Rot2.m_matrix.right.z + Rot1.m_matrix.up.y * Rot2.m_matrix.up.z + Rot1.m_matrix.up.z * Rot2.m_matrix.at.z + Rot2.m_matrix.pos.z),
						CVector(Rot1.m_matrix.at.x * Rot2.m_matrix.right.x + Rot1.m_matrix.at.y * Rot2.m_matrix.up.x + Rot1.m_matrix.at.z * Rot2.m_matrix.at.x + Rot2.m_matrix.pos.x,
								Rot1.m_matrix.at.x * Rot2.m_matrix.right.y + Rot1.m_matrix.at.y * Rot2.m_matrix.up.y + Rot1.m_matrix.at.z * Rot2.m_matrix.at.y + Rot2.m_matrix.pos.y,
								Rot1.m_matrix.at.x * Rot2.m_matrix.right.z + Rot1.m_matrix.at.y * Rot2.m_matrix.up.z + Rot1.m_matrix.at.z * Rot2.m_matrix.at.z + Rot2.m_matrix.pos.z),
						CVector(Rot1.m_matrix.pos.x * Rot2.m_matrix.right.x + Rot1.m_matrix.pos.y * Rot2.m_matrix.up.x + Rot1.m_matrix.pos.z * Rot2.m_matrix.at.x + Rot2.m_matrix.pos.x,
								Rot1.m_matrix.pos.x * Rot2.m_matrix.right.y + Rot1.m_matrix.pos.y * Rot2.m_matrix.up.y + Rot1.m_matrix.pos.z * Rot2.m_matrix.at.y + Rot2.m_matrix.pos.y,
								Rot1.m_matrix.pos.x * Rot2.m_matrix.right.z + Rot1.m_matrix.pos.y * Rot2.m_matrix.up.z + Rot1.m_matrix.pos.z * Rot2.m_matrix.at.z + Rot2.m_matrix.pos.z)); };

	friend inline CVector operator*(const CMatrix& m_matrix, const CVector& vec)
			{ return CVector(m_matrix.m_matrix.up.x * vec.y + m_matrix.m_matrix.right.x * vec.x + m_matrix.m_matrix.at.x * vec.z + m_matrix.m_matrix.pos.x,
								m_matrix.m_matrix.up.y * vec.y + m_matrix.m_matrix.right.y * vec.x + m_matrix.m_matrix.at.y * vec.z + m_matrix.m_matrix.pos.y,
								m_matrix.m_matrix.up.z * vec.y + m_matrix.m_matrix.right.z * vec.x + m_matrix.m_matrix.at.z * vec.z + m_matrix.m_matrix.pos.z); };

	friend inline CMatrix operator+(const CMatrix& Rot1, const CMatrix& Rot2)
			{ return CMatrix( Rot1.GetRight() + Rot2.GetRight(), Rot1.GetUp() + Rot2.GetUp(), Rot1.GetAt() + Rot2.GetAt(), Rot1.GetPos() + Rot2.GetPos() ); }

	inline CMatrix& operator=(const CMatrix& mat)
	{
		m_matrix = mat.m_matrix;
		if ( m_pMatrix != nullptr )
			UpdateRwMatrix(m_pMatrix);
		return *this;
	}

	inline CMatrix& operator+=(const CMatrix& mat)
	{
		GetRight() += mat.GetRight();
		GetUp() += mat.GetUp();
		GetAt() += mat.GetAt();
		GetPos() += mat.GetPos();

		return *this;
	}

	friend inline CMatrix& Invert(const CMatrix& src, CMatrix& dst)
	{
		dst.GetRight() = CVector(src.m_matrix.right.x, src.m_matrix.up.x, src.m_matrix.at.x);
		dst.GetUp() = CVector(src.m_matrix.right.y, src.m_matrix.up.y, src.m_matrix.at.y);
		dst.GetAt() = CVector(src.m_matrix.right.z, src.m_matrix.up.z, src.m_matrix.at.z);

		dst.GetPos() = -(dst.GetRight() * src.GetPos().x + dst.GetUp() * src.GetPos().y + dst.GetAt() * src.GetPos().z);

		return dst;
	}

	friend inline CMatrix Invert(const CMatrix& src)
	{
		CMatrix NewMatrix;
		Invert(src, NewMatrix);
		return NewMatrix;
	}

	friend inline CVector Multiply3x3(const CMatrix& m_matrix, const CVector& vec)
			{ return CVector(m_matrix.m_matrix.up.x * vec.y + m_matrix.m_matrix.right.x * vec.x + m_matrix.m_matrix.at.x * vec.z,
								m_matrix.m_matrix.up.y * vec.y + m_matrix.m_matrix.right.y * vec.x + m_matrix.m_matrix.at.y * vec.z,
								m_matrix.m_matrix.up.z * vec.y + m_matrix.m_matrix.right.z * vec.x + m_matrix.m_matrix.at.z * vec.z); };

	friend inline CVector Multiply3x3(const CVector& vec, const CMatrix& m_matrix)
			{ return CVector(DotProduct(m_matrix.GetRight(), vec), DotProduct(m_matrix.GetUp(), vec), DotProduct(m_matrix.GetAt(), vec)); }

	inline CVector&	GetRight()
		{ return *reinterpret_cast<CVector*>(&m_matrix.right); }
	inline CVector&	GetUp()
		{ return *reinterpret_cast<CVector*>(&m_matrix.up); }
	inline CVector&	GetAt()
		{ return *reinterpret_cast<CVector*>(&m_matrix.at); }
	inline CVector& GetPos()
		{ return *reinterpret_cast<CVector*>(&m_matrix.pos); }

	inline const CVector&	GetRight() const
	{ return *reinterpret_cast<const CVector*>(&m_matrix.right); }
	inline const CVector&	GetUp() const
	{ return *reinterpret_cast<const CVector*>(&m_matrix.up); }
	inline const CVector&	GetAt() const
	{ return *reinterpret_cast<const CVector*>(&m_matrix.at); }
	inline const CVector& GetPos() const
	{ return *reinterpret_cast<const CVector*>(&m_matrix.pos); }

	inline void		SetTranslateOnly(float fX, float fY, float fZ)
		{ m_matrix.pos.x = fX; m_matrix.pos.y = fY; m_matrix.pos.z = fZ; }
	
	inline void		SetRotateX(float fAngle)
		{ SetRotateXOnly(fAngle); m_matrix.pos.x = 0.0f; m_matrix.pos.y = 0.0f; m_matrix.pos.z = 0.0f; }
	inline void		SetRotateY(float fAngle)
		{ SetRotateYOnly(fAngle); m_matrix.pos.x = 0.0f; m_matrix.pos.y = 0.0f; m_matrix.pos.z = 0.0f; }
	inline void		SetRotateZ(float fAngle)
		{ SetRotateZOnly(fAngle); m_matrix.pos.x = 0.0f; m_matrix.pos.y = 0.0f; m_matrix.pos.z = 0.0f; }
	inline void		SetRotate(float fAngleX, float fAngleY, float fAngleZ)
		{ SetRotateOnly(fAngleX, fAngleY, fAngleZ); m_matrix.pos.x = 0.0f; m_matrix.pos.y = 0.0f; m_matrix.pos.z = 0.0f; }
	inline void		SetTranslate(float fX, float fY, float fZ)	
		{	m_matrix.right.x = 1.0f; m_matrix.right.y = 0.0f; m_matrix.right.z = 0.0f;
			m_matrix.up.x = 0.0f; m_matrix.up.y = 1.0f; m_matrix.up.z = 0.0f;
			m_matrix.at.x = 0.0f; m_matrix.at.y = 0.0f; m_matrix.at.z = 1.0f;
			SetTranslateOnly(fX, fY, fZ); }

	inline void		ResetOrientation()
	{	
		m_matrix.right.x = 1.0f; m_matrix.right.y = 0.0f; m_matrix.right.z = 0.0f;
		m_matrix.up.x = 0.0f; m_matrix.up.y = 1.0f; m_matrix.up.z = 0.0f;
		m_matrix.at.x = 0.0f; m_matrix.at.y = 0.0f; m_matrix.at.z = 1.0f;
	}

	inline void		SetUnity()
	{	
		ResetOrientation();
		m_matrix.pos.x = 0.0f; m_matrix.pos.y = 0.0f; m_matrix.pos.z = 0.0f;
	}

	inline void		SetScale(float fScale)
	{	
		m_matrix.right.x = fScale; m_matrix.right.y = 0.0f; m_matrix.right.z = 0.0f;
		m_matrix.up.x = 0.0f; m_matrix.up.y = fScale; m_matrix.up.z = 0.0f;
		m_matrix.at.x = 0.0f; m_matrix.at.y = 0.0f; m_matrix.at.z = fScale;
		m_matrix.pos.x = 0.0f; m_matrix.pos.y = 0.0f; m_matrix.pos.z = 0.0f;
	}

	inline void		RotateX(float fAngle)
	{
		CMatrix		RotationMatrix;
		RotationMatrix.SetRotateX(fAngle);
		*this *= RotationMatrix;
	}

	inline void		RotateY(float fAngle)
	{
		CMatrix		RotationMatrix;
		RotationMatrix.SetRotateY(fAngle);
		*this *= RotationMatrix;
	}

	inline void		RotateZ(float fAngle)
	{
		CMatrix		RotationMatrix;
		RotationMatrix.SetRotateZ(fAngle);
		*this *= RotationMatrix;
	}

	inline void		Rotate(float fAngleX, float fAngleY, float fAngleZ)
	{
		CMatrix		RotationMatrix;
		RotationMatrix.SetRotate(fAngleX, fAngleY, fAngleZ);
		*this *= RotationMatrix;
	}

	inline void		SetRotateXOnly(float fAngle)
	{
		m_matrix.right.x = 1.0f;
		m_matrix.right.y = 0.0f;
		m_matrix.right.z = 0.0f;

		m_matrix.up.x = 0.0f;
		m_matrix.up.y = cos(fAngle);
		m_matrix.up.z = sin(fAngle);

		m_matrix.at.x = 0.0f;
		m_matrix.at.y = -sin(fAngle);
		m_matrix.at.z = cos(fAngle);
	}

	inline void		SetRotateYOnly(float fAngle)
	{
		m_matrix.right.x = cos(fAngle);
		m_matrix.right.y = 0.0f;
		m_matrix.right.z = sin(fAngle);

		m_matrix.up.x = 0.0f;
		m_matrix.up.y = 1.0f;
		m_matrix.up.z = 0.0f;

		m_matrix.at.x = -sin(fAngle);
		m_matrix.at.y = 0.0f;
		m_matrix.at.z = cos(fAngle);
	}

	inline void		SetRotateZOnly(float fAngle)
	{
		m_matrix.at.x = 0.0f;
		m_matrix.at.y = 0.0f;
		m_matrix.at.z = 1.0f;

		m_matrix.up.x = -sin(fAngle);
		m_matrix.up.y = cos(fAngle);
		m_matrix.up.z = 0.0f;

		m_matrix.right.x = cos(fAngle);
		m_matrix.right.y = sin(fAngle);
		m_matrix.right.z = 0.0f;
	}

	inline void		SetRotateOnly(float fAngleX, float fAngleY, float fAngleZ)
	{
		m_matrix.right.x = cos(fAngleZ) * cos(fAngleY) - sin(fAngleZ) * sin(fAngleX) * sin(fAngleY);
		m_matrix.right.y = cos(fAngleZ) * sin(fAngleX) * sin(fAngleY) + sin(fAngleZ) * cos(fAngleY);
		m_matrix.right.z = -cos(fAngleX) * sin(fAngleY);

		m_matrix.up.x = -sin(fAngleZ) * cos(fAngleX);
		m_matrix.up.y = cos(fAngleZ) * cos(fAngleX);
		m_matrix.up.z = sin(fAngleX);

		m_matrix.at.x = sin(fAngleZ) * sin(fAngleX) * cos(fAngleY) + cos(fAngleZ) * sin(fAngleY);
		m_matrix.at.y = sin(fAngleZ) * sin(fAngleY) - cos(fAngleZ) * sin(fAngleX) * cos(fAngleY);
		m_matrix.at.z = cos(fAngleX) * cos(fAngleY);
	}

	inline void		Attach(RwMatrix* pMatrix, bool bHasMatrix)
	{
		if ( m_pMatrix && m_haveRwMatrix )
			RwMatrixDestroy(m_pMatrix);

		m_pMatrix = pMatrix;
		m_haveRwMatrix = bHasMatrix;

		Update();
	}

	inline void		AttachRw(RwMatrix* pMatrix, bool bHasMatrix)
	{
		if ( m_pMatrix && m_haveRwMatrix )
			RwMatrixDestroy(m_pMatrix);

		m_pMatrix = pMatrix;
		m_haveRwMatrix = bHasMatrix;

		UpdateRW();
	}

	inline void		Detach()
	{
		if ( m_pMatrix )
		{
			if ( m_haveRwMatrix )
				RwMatrixDestroy(m_pMatrix);
			m_pMatrix = nullptr;
		}
	}

	inline void		UpdateRW() const
	{
		if ( m_pMatrix )
			UpdateRwMatrix(m_pMatrix);
	}

	inline void		Update()
	{
		UpdateMatrix(m_pMatrix);
	}

	inline void		UpdateMatrix(RwMatrix* pMatrix)
	{
		m_matrix.right = pMatrix->right;
		m_matrix.up = pMatrix->up;
		m_matrix.at = pMatrix->at;
		m_matrix.pos = pMatrix->pos;
	}

	inline void		UpdateRwMatrix(RwMatrix* pMatrix) const
	{
		pMatrix->right = m_matrix.right;
		pMatrix->up = m_matrix.up;
		pMatrix->at = m_matrix.at;
		pMatrix->pos = m_matrix.pos;
		RwMatrixUpdate(pMatrix);
	}

	inline void		CopyToRwMatrix(RwMatrix* pMatrix) const
	{
		pMatrix->right = m_pMatrix->right;
		pMatrix->up = m_pMatrix->up;
		pMatrix->at = m_pMatrix->at;
		pMatrix->pos = m_pMatrix->pos;
		RwMatrixUpdate(pMatrix);
	}

	inline void		CopyOnlyMatrix(const CMatrix& from)
	{
		m_matrix = from.m_matrix;
	}
};

// These need to land here
inline CVector& CVector::FromMultiply(const CMatrix& mat, const CVector& vec)
{
	return *this = mat * vec;
}

inline CVector& CVector::FromMultiply3X3(const CMatrix& mat, const CVector& vec)
{
	return *this = Multiply3x3(mat, vec);
}

class CGeneral
{
public:
	static float GetRadianAngleBetweenPoints(float x1, float y1, float x2, float y2)
	{
		float x = x2 - x1;
		float y = y2 - y1;

		if (y == 0.0f)
		{
			y = 0.0001f;
		}

		if (x > 0.0f)
		{
			if (y > 0.0f)
			{
				return static_cast<float>(M_PI - std::atan2(x / y, 1.0f));
			}
			else
			{
				return -std::atan2(x / y, 1.0f);
			}
		}
		else
		{
			if (y > 0.0f)
			{
				return -static_cast<float>(M_PI + std::atan2(x / y, 1.0f));
			}
			else
			{
				return -std::atan2(x / y, 1.0f);
			}
		}
	}

	static float LimitRadianAngle(float angle)
	{
		while (angle >= M_PI)
		{
			angle -= static_cast<float>(2.0f * M_PI);
		}

		while (angle < -M_PI)
		{
			angle += static_cast<float>(2.0f * M_PI);
		}

		return angle;
	}

};
