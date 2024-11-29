#include "SVF.h"

#include <algorithm>
#include <map>

#ifndef _MSC_VER
#include <cstring>
#define _stricmp strcasecmp
#endif

namespace SVF
{
	Feature GetFeatureFromName( const char* featureName )
	{
		constexpr std::pair< const char*, Feature > features[] = {
#if _GTA_III || _GTA_VC
			{ "TAXI_LIGHT", Feature::TAXI_LIGHT },
			{ "SIT_IN_BOAT", Feature::SIT_IN_BOAT },
#endif

#if _GTA_VC
			{ "FBI_RANCHER_SIREN", Feature::FBI_RANCHER_SIREN },
			{ "FBI_WASHINGTON_SIREN", Feature::FBI_WASHINGTON_SIREN },
			{ "VICE_CHEETAH_SIREN", Feature::VICE_CHEETAH_SIREN },
			{ "DRAW_BACKFACES", Feature::DRAW_BACKFACES },
#endif

#if _GTA_SA
			{ "PHOENIX_FLUTTER", Feature::PHOENIX_FLUTTER },
			{ "SWEEPER_BRUSHES", Feature::SWEEPER_BRUSHES },
			{ "NEWSVAN_DISH", Feature::NEWSVAN_DISH },
			{ "EXTRA_AILERONS1", Feature::EXTRA_AILERONS1 },
			{ "EXTRA_AILERONS2", Feature::EXTRA_AILERONS2 },
			{ "DOUBLE_TRAILER", Feature::DOUBLE_TRAILER },
			{ "VORTEX_EXHAUST", Feature::VORTEX_EXHAUST },
			{ "TOWTRUCK_HOOK", Feature::TOWTRUCK_HOOK },
			{ "TRACTOR_HOOK", Feature::TRACTOR_HOOK },
			{ "RHINO_WHEELS", Feature::RHINO_WHEELS },
			{ "FIRELA_LADDER", Feature::FIRELA_LADDER },
#endif
		};

		auto it = std::find_if( std::begin(features), std::end(features), [featureName]( const auto& e ) {
			return _stricmp( e.first, featureName ) == 0;
		});

		if ( it == std::end(features) ) return Feature::NO_FEATURE;
		return it->second;
	}

	static int32_t nextFeatureCookie = 0;
	int32_t _getCookie()
	{
		return nextFeatureCookie++;
	}

	static int32_t highestStockCookie = 0;
	int32_t _getCookieStockID()
	{
		return highestStockCookie = _getCookie();
	}

	auto _registerFeatureInternal( int32_t modelID, Feature feature )
	{
		return std::make_pair( modelID, std::make_tuple( feature, _getCookieStockID() ) );
	}

	static std::multimap<int32_t, std::tuple<Feature, int32_t> > specialVehFeatures = {
#if _GTA_III
		_registerFeatureInternal( 110, Feature::TAXI_LIGHT ),
		_registerFeatureInternal( 142, Feature::SIT_IN_BOAT ),
#elif _GTA_VC
		_registerFeatureInternal( 147, Feature::FBI_WASHINGTON_SIREN ),
		_registerFeatureInternal( 150, Feature::TAXI_LIGHT ),
		_registerFeatureInternal( 220, Feature::FBI_RANCHER_SIREN ),
		_registerFeatureInternal( 236, Feature::VICE_CHEETAH_SIREN ),
#elif _GTA_SA
		_registerFeatureInternal( 432, Feature::RHINO_WHEELS ),
		_registerFeatureInternal( 511, Feature::EXTRA_AILERONS1 ),
		_registerFeatureInternal( 513, Feature::EXTRA_AILERONS2 ),
		_registerFeatureInternal( 525, Feature::TOWTRUCK_HOOK ),
		_registerFeatureInternal( 531, Feature::TRACTOR_HOOK ),
		_registerFeatureInternal( 539, Feature::VORTEX_EXHAUST ),
		_registerFeatureInternal( 544, Feature::FIRELA_LADDER ),
		_registerFeatureInternal( 574, Feature::SWEEPER_BRUSHES ),
		_registerFeatureInternal( 582, Feature::NEWSVAN_DISH ),
		_registerFeatureInternal( 591, Feature::DOUBLE_TRAILER ),
		_registerFeatureInternal( 603, Feature::PHOENIX_FLUTTER ),
#endif
	};

	static std::multimap<std::string, std::tuple<Feature, int32_t> > specialVehFeaturesByName;
	static void* (*GetModelInfoCB)(const char* name, int* outIndex);
	static bool bModelNamesReady = false;

	void _resolveFeatureNamesInternal()
	{
		if (bModelNamesReady && GetModelInfoCB != nullptr && !specialVehFeaturesByName.empty())
		{
			for (const auto& feature : specialVehFeaturesByName)
			{
				int32_t index;
				if (GetModelInfoCB(feature.first.c_str(), &index) != nullptr)
				{
					specialVehFeatures.emplace(index, feature.second);
				}
			}
			specialVehFeaturesByName.clear();
		}
	}

	int32_t RegisterFeature( int32_t modelID, Feature feature )
	{
		if ( feature == Feature::NO_FEATURE ) return -1;

		const int32_t cookie = _getCookie();
		specialVehFeatures.emplace( modelID, std::make_tuple(feature, cookie) );
		return cookie;
	}

	int32_t RegisterFeature( std::string modelName, Feature feature )
	{
		if ( feature == Feature::NO_FEATURE ) return -1;

		const int32_t cookie = _getCookie();
		specialVehFeaturesByName.emplace( std::move(modelName), std::make_tuple(feature, cookie) );
		return cookie;
	}

	void DeleteFeature( int32_t cookie )
	{
		for ( auto it = specialVehFeatures.begin(); it != specialVehFeatures.end(); ++it )
		{
			if ( std::get<int32_t>(it->second) == cookie )
			{
				specialVehFeatures.erase( it );
				return;
			}
		}

		for ( auto it = specialVehFeaturesByName.begin(); it != specialVehFeaturesByName.end(); ++it )
		{
			if ( std::get<int32_t>(it->second) == cookie )
			{
				specialVehFeaturesByName.erase( it );
				return;
			}
		}
	}

	void DisableStockVehiclesForFeature( Feature feature )
	{
		if ( feature == Feature::NO_FEATURE ) return;
		for ( auto it = specialVehFeatures.begin(); it != specialVehFeatures.end(); )
		{
			if ( std::get<Feature>(it->second) == feature && std::get<int32_t>(it->second) <= highestStockCookie )
			{
				it = specialVehFeatures.erase( it );
			}
			else
			{
				++it;
			}
		}

		for ( auto it = specialVehFeaturesByName.begin(); it != specialVehFeaturesByName.end(); )
		{
			if ( std::get<Feature>(it->second) == feature && std::get<int32_t>(it->second) <= highestStockCookie )
			{
				it = specialVehFeaturesByName.erase( it );
			}
			else
			{
				++it;
			}
		}
	}

	bool ModelHasFeature( int32_t modelID, Feature feature )
	{
		_resolveFeatureNamesInternal();
		auto results = specialVehFeatures.equal_range( modelID );
		return std::find_if( results.first, results.second, [feature] ( const auto& e ) {
			return std::get<Feature>(e.second) == feature;
		} ) != results.second;
	}

	std::function<bool(Feature)> ForAllModelFeatures( int32_t modelID, std::function<bool(Feature)> pred )
	{
		_resolveFeatureNamesInternal();
		auto results = specialVehFeatures.equal_range( modelID );
		for ( auto it = results.first; it != results.second; ++it )
		{
			if (!pred(std::get<Feature>(it->second)))
			{
				break;
			}
		}
		return pred;
	}

	void RegisterGetModelInfoCB(void*(*func)(const char*, int*))
	{
		GetModelInfoCB = func;
	}

	void MarkModelNamesReady()
	{
		bModelNamesReady = true;
	}
}

// Returns "feature cookie" on success, -1 on failure
extern "C" {
	
__declspec(dllexport) int32_t RegisterSpecialVehicleFeature( int32_t modelID, const char* featureName )
{
	if ( featureName == nullptr ) return -1;
	return SVF::RegisterFeature( modelID, SVF::GetFeatureFromName(featureName) );
}

__declspec(dllexport) int32_t RegisterSpecialVehicleFeatureByName( const char* modelName, const char* featureName )
{
	if ( featureName == nullptr || modelName == nullptr ) return -1;
	return SVF::RegisterFeature( modelName, SVF::GetFeatureFromName(featureName) );
}

__declspec(dllexport) void DeleteSpecialVehicleFeature( int32_t cookie )
{
	if ( cookie == -1 ) return;
	SVF::DeleteFeature( cookie );
}

__declspec(dllexport) void DisableStockVehiclesForSpecialVehicleFeature( const char* featureName )
{
	if ( featureName == nullptr ) return;
	SVF::DisableStockVehiclesForFeature( SVF::GetFeatureFromName(featureName) );
}

}
