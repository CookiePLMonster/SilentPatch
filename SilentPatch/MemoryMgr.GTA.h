#pragma once

#include "Utils/MemoryMgr.h"

#include <cassert>
#include <type_traits>
#include <variant>
#include "Utils/Patterns.h"

namespace Memory
{
	struct PatternAndOffset
	{
		constexpr PatternAndOffset( std::string_view pattern, ptrdiff_t offset = 0 ) noexcept
			: pattern(std::move(pattern)), offset(offset)
		{
		}

		constexpr bool Valid() const noexcept { return !pattern.empty(); }

		std::string_view pattern;
		ptrdiff_t offset;
	};

	using AddrVariant = std::variant<intptr_t, PatternAndOffset>;

	namespace internal
	{
		inline int8_t* GetVer()
		{
			static int8_t	bVer = -1;
			return &bVer;
		}

		inline bool* GetEuropean()
		{
			static bool			bEuropean;
			return &bEuropean;
		}
	}
}

namespace Memory
{
	namespace internal
	{
		inline uintptr_t HandlePattern( const PatternAndOffset& pattern )
		{
			hook::pattern p(pattern.pattern);
			if (p.count_hint(1).size() == 1)
			{
				return reinterpret_cast<uintptr_t>(p.get_first<const void>(pattern.offset));
			}
			return 0u;
		}

#if defined _GTA_III
		inline void InitializeVersions()
		{
			signed char*	bVer = GetVer();

			if ( *bVer == -1 )
			{
				if (*(uint32_t*)DynBaseAddress(0x5C1E75) == 0xB85548EC) *bVer = 0;
				else if (*(uint32_t*)DynBaseAddress(0x5C2135) == 0xB85548EC) *bVer = 1;
				else if (*(uint32_t*)DynBaseAddress(0x5C6FD5) == 0xB85548EC) *bVer = 2;
			}
		}

#elif defined _GTA_VC

		inline void InitializeVersions()
		{
			signed char*	bVer = GetVer();

			if ( *bVer == -1 )
			{
				if (*(uint32_t*)DynBaseAddress(0x667BF5) == 0xB85548EC) *bVer = 0;
				else if (*(uint32_t*)DynBaseAddress(0x667C45) == 0xB85548EC) *bVer = 1;
				else if (*(uint32_t*)DynBaseAddress(0x666BA5) == 0xB85548EC) *bVer = 2;
			}
		}

#elif defined _GTA_SA

		inline bool TryMatch_10()
		{
			if ( *(uint32_t*)DynBaseAddress(0x82457C) == 0x94BF )
			{
				// 1.0 US
				*GetVer() = 0;
				*GetEuropean() = false;
				return true;
			}
			if ( *(uint32_t*)DynBaseAddress(0x8245BC) == 0x94BF )
			{
				// 1.0 EU
				*GetVer() = 0;
				*GetEuropean() = true;
				return true;
			}
			return false;
		}

		inline bool TryMatch_11()
		{
			if ( *(uint32_t*)DynBaseAddress(0x8252FC) == 0x94BF )
			{
				// 1.01 US
				*GetVer() = 1;
				*GetEuropean() = false;
				return true;
			}
			if ( *(uint32_t*)DynBaseAddress(0x82533C) == 0x94BF )
			{
				// 1.01 EU
				*GetVer() = 1;
				*GetEuropean() = true;
				return true;
			}
			return false;
		}

		inline bool TryMatch_30()
		{
			if (*(uint32_t*)DynBaseAddress(0x85EC4A) == 0x94BF )
			{
				// 3.0
				*GetVer() = 2;
				*GetEuropean() = false;
				return true;
			}
			return false;
		}

		inline bool TryMatch_newsteam_r1()
		{
			if ( *(uint32_t*)DynBaseAddress(0x858D21) == 0x3539F633 )
			{
				// newsteam r1
				*GetVer() = 3;
				*GetEuropean() = false;
				return true;
			}
			return false;
		}

		inline bool TryMatch_newsteam_r2()
		{
			if ( *(uint32_t*)DynBaseAddress(0x858D51) == 0x3539F633 )
			{
				// newsteam r2
				*GetVer() = 4;
				*GetEuropean() = false;
				return true;
			}
			return false;
		}

		inline bool TryMatch_newsteam_r2_lv()
		{
			if ( *(uint32_t*)DynBaseAddress(0x858C61) == 0x3539F633 )
			{
				// newsteam r2 lv
				*GetVer() = 5;
				*GetEuropean() = false;
				return true;
			}
			return false;
		}

		inline bool TryMatch_RGL()
		{
			if ( *(uint32_t*)DynBaseAddress(0x858501) == 0x3539F633 )
			{
				// RGL (1.0.22.0)
				*GetVer() = 6;
				*GetEuropean() = false;
				return true;
			}
			return false;
		}

		inline void InitializeVersions()
		{
			if ( *GetVer() == -1 )
			{
				if ( TryMatch_10() ) return;
				if ( TryMatch_11() ) return;
				if ( TryMatch_30() ) return;
				if ( TryMatch_newsteam_r1() ) return;
				if ( TryMatch_newsteam_r2() ) return;
				if ( TryMatch_newsteam_r2_lv() ) return;
				if ( TryMatch_RGL() ) return;

				// If not matched, from now on we assume this is a "future" EXE
				// and try to use newsteam/RGL patterns anyway
				*GetVer() = INT8_MAX;
			}
		}

		inline void InitializeRegion_10()
		{
			signed char*	bVer = GetVer();

			if ( *bVer == -1 )
			{
				if ( !TryMatch_10() )
				{
		#ifdef assert
					assert(!"AddressByRegion_10 on non-1.0 EXE!");
		#endif
				}
			}
		}

		inline void InitializeRegion_11()
		{
			signed char*	bVer = GetVer();

			if ( *bVer == -1 )
			{
				if ( !TryMatch_11() )
				{
					assert(!"AddressByRegion_11 on non-1.01 EXE!");
				}
			}
		}

		inline uintptr_t AdjustAddress_10(uintptr_t address10)
		{
			if ( *GetEuropean() )
			{		
				if ( address10 >= 0x746720 && address10 < 0x857000 )
				{
					if ( address10 >= 0x7BA940 )
						address10 += 0x40;
					else
						address10 += 0x50;
				}
			}
			return address10;
		}

		inline uintptr_t AdjustAddress_11(uintptr_t address11)
		{
			if ( !(*GetEuropean()) && address11 > 0x746FA0 )
			{
				if ( address11 < 0x7BB240 )
					address11 -= 0x50;
				else
					address11 -= 0x40;
			}
			return address11;
		}

		inline uintptr_t AddressByVersion(AddrVariant address10, AddrVariant address11, AddrVariant addressSteam, PatternAndOffset patternNewExes)
		{
			InitializeVersions();

			signed char	bVer = *GetVer();

			switch ( bVer )
			{
			case 0:
				if ( auto pao = std::get_if<PatternAndOffset>(&address10) ) return HandlePattern( *pao );
				else
				{
					const uintptr_t addr = std::make_unsigned_t<intptr_t>(*std::get_if<intptr_t>(&address10));
					if (addr == 0)
					{
						assert(false);
						return 0;
					}

					// Adjust to EU if needed
					return AdjustAddress_10(addr);
				}
				break;
			case 1:
				if ( auto pao = std::get_if<PatternAndOffset>(&address11) ) return HandlePattern( *pao );
				else
				{
					const uintptr_t addr = std::make_unsigned_t<intptr_t>(*std::get_if<intptr_t>(&address11));
					if (addr == 0)
					{
						assert(false);
						return 0;
					}

					// Adjust to US if needed
					return AdjustAddress_11(addr);
				}
			case 2:
				if ( auto pao = std::get_if<PatternAndOffset>(&addressSteam) ) return HandlePattern( *pao );
				else
				{
					const uintptr_t addr = std::make_unsigned_t<intptr_t>(*std::get_if<intptr_t>(&addressSteam));
					if (addr == 0)
					{
						assert(false);
						return 0;
					}

					return addr;
				}
			default:
				if ( !patternNewExes.Valid() )
					return 0;

				return HandlePattern( patternNewExes );
			}
		}

		inline uintptr_t AddressByRegion_10(uintptr_t address10)
		{
			InitializeRegion_10();

			// Adjust to EU if needed
			return AdjustAddress_10(address10);
		}

		inline uintptr_t AddressByRegion_11(uintptr_t address11)
		{
			InitializeRegion_11();

			// Adjust to US if needed
			return AdjustAddress_11(address11);
		}

#else

		inline void InitializeVersions()
		{
		}

#endif

#if defined _GTA_III || defined _GTA_VC

		inline uintptr_t AddressByVersion(uintptr_t address10, uintptr_t address11, uintptr_t addressSteam)
		{
			InitializeVersions();

			signed char		bVer = *GetVer();

			switch ( bVer )
			{
			case 1:
#ifdef assert
				assert(address11);
#endif
				return DynBaseAddress(address11);
			case 2:
#ifdef assert
				assert(addressSteam);
#endif
				return DynBaseAddress(addressSteam);
			default:
#ifdef assert
				assert(address10);
#endif
				return DynBaseAddress(address10);
			}
		}

#endif

	}
}

#if defined _GTA_III || defined _GTA_VC

template<typename T = uintptr_t>
inline T AddressByVersion(uintptr_t address10, uintptr_t address11, uintptr_t addressSteam)
{
	return T(Memory::internal::AddressByVersion( address10, address11, addressSteam ));
}

#elif defined _GTA_SA

template<typename T = uintptr_t>
inline T AddressByVersion(Memory::AddrVariant address10, Memory::AddrVariant address11, Memory::AddrVariant addressSteam)
{
	return T(Memory::internal::AddressByVersion( std::move(address10), std::move(address11), std::move(addressSteam), Memory::PatternAndOffset(std::string_view()) ));
}

template<typename T = uintptr_t>
inline T AddressByVersion(Memory::AddrVariant address10, Memory::AddrVariant address11, Memory::AddrVariant addressSteam, Memory::PatternAndOffset patternNewExes)
{
	return T(Memory::internal::AddressByVersion( std::move(address10), std::move(address11), std::move(addressSteam), std::move(patternNewExes) ));
}

template<typename T = uintptr_t>
inline T AddressByVersion(Memory::AddrVariant address10, Memory::PatternAndOffset patternNewExes)
{
	return T(Memory::internal::AddressByVersion( std::move(address10), 0, 0, std::move(patternNewExes) ));
}

template<typename T = uintptr_t>
inline T AddressByRegion_10(uintptr_t address10)
{
	return T(Memory::internal::AddressByRegion_10(address10));
}

template<typename T = uintptr_t>
inline T AddressByRegion_11(uintptr_t address11)
{
	return T(Memory::internal::AddressByRegion_11(address11));
}

#endif

namespace Memory
{
	struct VersionInfo
	{
		int8_t version;
		bool european;
	};

	inline VersionInfo GetVersion()
	{
		Memory::internal::InitializeVersions();
		return { *Memory::internal::GetVer(), *Memory::internal::GetEuropean() };
	}
};
