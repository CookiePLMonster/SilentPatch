#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace SVF
{
	enum class Feature
	{
		NO_FEATURE,

#if _GTA_III || _GTA_VC
		TAXI_LIGHT, // Corrected light placement for Taxi
		SIT_IN_BOAT, // Sitting animation in the boat, used in Speeder
#endif

#if _GTA_VC
		FBI_RANCHER_SIREN,
		FBI_WASHINGTON_SIREN,
		VICE_CHEETAH_SIREN,

		DRAW_BACKFACES, // Not vehicle specific, but it'll do
#endif

#if _GTA_III || _GTA_VC
		_INTERNAL_NO_SPECULARITY_ON_EXTRAS, // Band-aid fix for leather roofs being reflective now that envmaps are applied on them
#endif

#if _GTA_SA
		// Those are fully controlled by SilentPatch
		PHOENIX_FLUTTER,
		SWEEPER_BRUSHES,
		NEWSVAN_DISH,
		EXTRA_AILERONS1, // Like on Beagle
		EXTRA_AILERONS2, // Like on Stuntplane
		DOUBLE_TRAILER, // Like on artict3

		// Those are partially controlled by SilentPatch (only affected by minor fixes)
		VORTEX_EXHAUST,
		TOWTRUCK_HOOK,
		TRACTOR_HOOK,
		RHINO_WHEELS,
		FIRELA_LADDER,

		// Internal SP use only, formerly "rotor exceptions"
		// Unreachable from RegisterSpecialVehicleFeature
		_INTERNAL_NO_ROTOR_FADE,
		_INTERNAL_NO_LIGHTBEAM_BFC_FIX,
		_INTERNAL_FORCE_DOUBLE_RWHEELS_OFF,
		_INTERNAL_FORCE_DOUBLE_RWHEELS_ON,
#endif
	};

	int32_t RegisterFeature( int32_t modelID, Feature feature );
	int32_t RegisterFeature( std::string modelName, Feature feature );
	void DeleteFeature( int32_t cookie );
	void DisableStockVehiclesForFeature( Feature feature );
	bool ModelHasFeature( int32_t modelID, Feature feature );
	std::function<bool(Feature)> ForAllModelFeatures( int32_t modelID, std::function<bool(Feature)> pred );

	void RegisterGetModelInfoCB(void*(*func)(const char*, int*));
	void MarkModelNamesReady();
};
