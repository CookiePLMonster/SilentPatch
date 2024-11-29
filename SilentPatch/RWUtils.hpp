#pragma once

#include <rwcore.h>
#include <rpworld.h>

#define RwEngineInstance (*rwengine)

extern void** rwengine;

template<RwRenderState State>
class RwScopedRenderState
{
public:
	explicit RwScopedRenderState()
	{
		[[maybe_unused]] RwBool result = RwRenderStateGet( State, &m_value );
		assert( result != FALSE );
	}

	~RwScopedRenderState()
	{
		[[maybe_unused]] RwBool result = RwRenderStateSet( State, m_value );
		assert( result != FALSE );
	}

	RwScopedRenderState( const RwScopedRenderState& ) = delete;

private:
	void* m_value = nullptr;
};

template <typename Pred>
Pred RwFrameForAllChildren(RwFrame* frame, Pred callback)
{
	for ( RwFrame* curFrame = frame->child; curFrame != nullptr; curFrame = curFrame->next )
	{
		if ( callback(curFrame) == nullptr )
			break;
	}
	return callback;
}

template <typename Pred>
Pred RwFrameForAllObjects(RwFrame* frame, Pred callback)
{
	for ( RwLLLink* link = rwLinkListGetFirstLLLink(&frame->objectList); link != rwLinkListGetTerminator(&frame->objectList); link = rwLLLinkGetNext(link) )
	{
		if ( callback(&rwLLLinkGetData(link, RwObjectHasFrame, lFrame)->object) == nullptr )
			break;
	}

	return callback;
}

template <typename Pred>
Pred RpClumpForAllAtomics(RpClump* clump, Pred callback)
{
	for ( RwLLLink* link = rwLinkListGetFirstLLLink(&clump->atomicList); link != rwLinkListGetTerminator(&clump->atomicList); link = rwLLLinkGetNext(link) )
	{
		if ( callback(rwLLLinkGetData(link, RpAtomic, inClumpLink)) == nullptr )
			break;
	}
	return callback;
}

template <typename Pred>
Pred RpGeometryForAllMaterials(RpGeometry* geometry, Pred callback)
{
	for ( RwInt32 i = 0, j = geometry->matList.numMaterials; i < j; i++ )
	{
		if ( callback(geometry->matList.materials[i]) == nullptr )
			break;
	}
	return callback;
}
