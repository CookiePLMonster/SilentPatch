dofile "premake/vcpkg.lua"
dofile "premake/versionmeta.lua"
dofile "premake/renderware.lua"

newoption {
	trigger = "modern-toolset",
	description = "Build with the default toolset instead of v141_xp. The modules will not load on Windows XP."
}

newoption {
	trigger = "projects",
	value = "SET",
	description = "Which projects to generate (default: all)",
	allowed = {
		{ "all",   "Every project" },
		{ "games", "The three games, without the ddraw proxy" },
		{ "iii",   "GTA III only" },
		{ "vc",    "Vice City only" },
		{ "sa",    "San Andreas only" },
		{ "ddraw", "The ddraw proxy only" },
	}
}

-- Each game needs its own RenderWare SDK, and contributors rarely have all
-- three, so generating a subset has to be possible.
local SELECTION = _OPTIONS["projects"] or "all"

local function wanted(name)
	if SELECTION == "all" then return true end
	if SELECTION == "games" then return name ~= "ddraw" end
	return SELECTION == name
end

local function toolset_for_configuration()
	if _OPTIONS["modern-toolset"] then
		return nil
	end
	return "msc-v141_xp"
end

-- Files shared between projects. Each game pulls in a different subset, and
-- with different defines, so //SilentPatch is compiled into every module
-- rather than built once as a library.
local SHARED = {
	base = {
		"SilentPatch/Desktop.cpp", "SilentPatch/Desktop.h",
		"SilentPatch/Utils/Patterns.cpp", "SilentPatch/Utils/Patterns.h",
		"SilentPatch/Utils/MemoryMgr.h",
	},
	ddraw = {
		"SilentPatch/Common_ddraw.cpp", "SilentPatch/Common_ddraw.h",
	},
	game = {
		"SilentPatch/ParseUtils.cpp", "SilentPatch/ParseUtils.hpp",
		"SilentPatch/SVF.cpp", "SilentPatch/SVF.h",
		"SilentPatch/TheFLAUtils.cpp", "SilentPatch/TheFLAUtils.h",
		"SilentPatch/Maths.h", "SilentPatch/MemoryMgr.GTA.h",
		"SilentPatch/Random.h", "SilentPatch/RWUtils.hpp",
		"SilentPatch/debugmenu_public.h",
	},
	iii_vc = {
		"SilentPatch/Common.cpp", "SilentPatch/Common.h",
		"SilentPatch/RWGTA.cpp", "SilentPatch/RWGTA.h",
		"SilentPatch/StoredCar.cpp", "SilentPatch/StoredCar.h",
		"SilentPatch/Timer.cpp", "SilentPatch/Timer.h",
		"SilentPatch/ExternalBindings.hpp", "SilentPatch/StdAfx.h",
	},
	sa = {
		"SilentPatch/FriendlyMonitorNames.cpp", "SilentPatch/FriendlyMonitorNames.h",
	},
	resources = {
		"SilentPatch/SilentPatch.rc", "SilentPatch/resource1.h",
		"SilentPatch/ExternalBindings.natvis",
	},
}

-- Translation units that must not use the precompiled header, matching the
-- PrecompiledHeader=NotUsing entries in the .vcxproj files.
local NO_PCH = {
	"SilentPatch/Common.cpp",
	"SilentPatch/Common_ddraw.cpp",
	"SilentPatch/Desktop.cpp",
	"SilentPatch/FriendlyMonitorNames.cpp",
	"SilentPatch/ParseUtils.cpp",
	"SilentPatch/RWGTA.cpp",
	"SilentPatch/SVF.cpp",
	"SilentPatch/TheFLAUtils.cpp",
	"SilentPatch/Utils/Patterns.cpp",
}

local function no_pch()
	for _, file in ipairs(NO_PCH) do
		filter { "files:" .. file }
			enablepch "off"
	end
	filter {}
end

local function module(name, dir)
	local meta = version_info(dir)

	kind "SharedLib"
	language "C++"
	targetname(meta.SILENTPATCH_NAME)
	targetextension(meta.SILENTPATCH_EXT)
	includedirs { "SilentPatch", dir }
end


workspace "SilentPatch"
	platforms { "Win32" }

if wanted("iii") then
project "SilentPatchIII"
	module("SilentPatchIII", "SilentPatchIII")
	defines { "_GTA_III" }
	renderware_includedirs "III"

	files(SHARED.base)
	files(SHARED.ddraw)
	files(SHARED.game)
	files(SHARED.iii_vc)
	files(SHARED.resources)
	files { "SilentPatchIII/*.cpp", "SilentPatchIII/*.h" }

	pchheader "StdAfx.h"
	pchsource "SilentPatchIII/StdAfxIII.cpp"
	no_pch()

end

if wanted("vc") then
project "SilentPatchVC"
	module("SilentPatchVC", "SilentPatchVC")
	defines { "_GTA_VC" }
	renderware_includedirs "VC"

	files(SHARED.base)
	files(SHARED.ddraw)
	files(SHARED.game)
	files(SHARED.iii_vc)
	files(SHARED.resources)
	files { "SilentPatchVC/*.cpp", "SilentPatchVC/*.h" }

	pchheader "StdAfx.h"
	pchsource "SilentPatchVC/StdAfxVC.cpp"
	no_pch()

end

if wanted("sa") then
project "SilentPatchSA"
	module("SilentPatchSA", "SilentPatchSA")
	defines { "_GTA_SA" }
	renderware_includedirs "SA"

	files(SHARED.base)
	files(SHARED.game)
	files(SHARED.sa)
	files(SHARED.resources)
	files { "SilentPatchSA/*.cpp", "SilentPatchSA/*.h", "SilentPatchSA/SilentPatchSA.rc" }

	pchheader "StdAfxSA.h"
	pchsource "SilentPatchSA/StdAfxSA.cpp"
	no_pch()

	-- libflac, restored from SilentPatchSA/vcpkg.json. The generated project
	-- does not sit next to that manifest, so vcpkg is told where it is.
	vcpkg "On"
	vcpkgmanifest "On"
	vcpkgstatic "On"
	vcpkgmanifestroot "$(MSBuildProjectDirectory)\\..\\SilentPatchSA"

	filter "configurations:Shipping"
		defines { "_SECURE_SCL=0" }
	filter {}

end

if wanted("ddraw") then
project "DDraw"
	module("DDraw", "DDraw")

	-- The version resource calls this SilentPatchDDraw, but the file on disk
	-- has to be named ddraw.dll for the games to load it at all.
	targetname "ddraw"

	files(SHARED.base)
	files(SHARED.ddraw)
	files(SHARED.resources)
	files { "DDraw/dllmain.cpp" }

	-- A ddraw.dll proxy the games load on startup, which gets SilentPatch in
	-- early enough to fix things that happen before an ASI loader would run.
	linkoptions { "/DEF:\"%{wks.location}/../DDraw/DDraw.def\"" }


end

workspace "*"
	configurations { "Debug", "Release", "Shipping" }
	location "build"

	vpaths {
		["Headers/*"] = { "**.h", "**.hpp" },
		["Sources/*"] = { "**.c", "**.cpp" },
		["Resources"] = { "**.rc", "**.def", "**.natvis" },
	}

	toolset(toolset_for_configuration())

	cppdialect "C++17"
	characterset "Unicode"
	staticruntime "on"
	rtti "off"
	symbols "on"
	warnings "Extra"
	conformancemode "on"
	vectorextensions "IA32"

	buildoptions { "/sdl", "/Zc:strictStrings", "/Zc:threadSafeInit-" }

	defines { "_HAS_EXCEPTIONS=0", "PATTERNS_USE_HINTS=1" }

	-- Loaded very early in the host process, so these are taken lazily.
	linkoptions { "/LARGEADDRESSAWARE", "/DELAYLOAD:shell32.dll", "/DELAYLOAD:shlwapi.dll" }
	links { "delayimp" }

filter "configurations:Debug"
	runtime "Debug"

-- NDEBUG is deliberately absent from Release. SilentPatch.rc keys the major
-- version off it, so Release builds identify as 0.1.<revision> and Shipping as
-- 1.1.<revision> - which is how a build from a working tree is told apart from
-- a published one.
filter "configurations:Shipping"
	defines { "NDEBUG" }
	linkoptions { "/pdbaltpath:%_PDB%" }

filter "configurations:not Debug"
	optimize "Speed"
	functionlevellinking "on"
	linktimeoptimization "on"
	inlining "auto"			-- InlineFunctionExpansion=AnySuitable
	omitframepointer "on"
	buildoptions { "/Ot" }		-- FavorSizeOrSpeed=Speed

filter { "platforms:Win32" }
	system "Windows"
	architecture "x86"
