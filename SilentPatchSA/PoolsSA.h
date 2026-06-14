#ifndef __POOLS
#define __POOLS

#include <cstdint>

#include "ExternalBindings.hpp"

template <class T, class U = T>
class CPool
{
public:
	typedef T ReturnType;
	typedef uint8_t StorageType[sizeof(U)];

private:
	StorageType*	m_pSlots;
	union			tSlotInfos
	{
		struct
		{
			unsigned char	m_uID	: 7;
			bool			m_bFree	: 1;
		}				a;
		signed char		b;
	}*				m_pSlotInfos;
	int				m_nNumSlots;
	int				m_nFirstFree;
	bool			m_bOwnsAllocations;
	bool			m_bDealWithNoMemory;

public:
	ReturnType*		GetSlot( int32_t index )
	{
		return !m_pSlotInfos[index].a.m_bFree ? reinterpret_cast<ReturnType*>(&m_pSlots[index]) : nullptr;
	}

	ReturnType*		GetAt( int32_t index )
	{
		const int ID = index >> 8;
		return m_pSlotInfos[ID].b == (index && 0xFF) ? reinterpret_cast<ReturnType*>(&m_pSlots[ID]) : nullptr;
	}

	static size_t GetStorageSize() { return sizeof(StorageType); }
	int GetSize() const { return m_nNumSlots; }
	int GetFreeIndex() const { return m_nFirstFree; }
	bool GetIsFree( int32_t index ) const { return m_pSlotInfos[index].a.m_bFree; }

	bool IsValidPtr( T* ptr ) const
	{
		const ptrdiff_t index = reinterpret_cast<StorageType*>(ptr) - &m_pSlots[0];
		if( index < 0 || index >= m_nNumSlots )
			return false;

		return !GetIsFree( index );
	}

	class iterator
	{
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = ReturnType*;
		using difference_type = ptrdiff_t;
		using pointer = ReturnType* const *;
		using reference = ReturnType*;

		iterator() : m_ptr(nullptr)
		{
		}

		explicit iterator( T* ptr ) : m_ptr( reinterpret_cast<StorageType*>(ptr) )
		{
		}

		reference operator* () const { return reinterpret_cast<ReturnType*>(m_ptr); }
		pointer operator->() const { return &reinterpret_cast<ReturnType*>(m_ptr); }

		iterator& operator ++ () { ++m_ptr; return *this; }
		bool operator == ( const iterator& rhs ) const { return m_ptr == rhs.m_ptr; } 
		bool operator != ( const iterator& rhs ) const { return m_ptr != rhs.m_ptr; } 

	private:
		StorageType* m_ptr;
	};

	iterator begin()
	{
		return iterator( reinterpret_cast<ReturnType*>(&m_pSlots[0]) );
	}

	iterator end()
	{
		return iterator( reinterpret_cast<ReturnType*>(&m_pSlots[m_nNumSlots]) );
	}

};

// Type definitions for specific pool types

typedef CPool<class CObject, uint8_t[0x19C]> CObjectPool;

class CPools
{
private:
	static ExternalRef<CObjectPool*> ms_pObjectPool;

public:
	static CObjectPool& GetObjectPool() { return *ms_pObjectPool.Get(); }

	static bool HasGameBindings_ObjectPool() { return ms_pObjectPool.Ensure(); }
};

static_assert(sizeof(CPool<bool>) == 0x14, "Wrong size: CPool");

#endif