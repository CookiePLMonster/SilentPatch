-- Locates the RenderWare Graphics SDKs. Each game is built against the
-- RenderWare version it originally shipped with, so there is one SDK per game
-- rather than a single shared one.
--
-- These cannot be downloaded: RenderWare Graphics is proprietary Criterion/EA
-- middleware that was never distributed publicly. All this does is find a copy
-- that is already on the machine. Only headers are needed - SilentPatch never
-- links against RenderWare, it resolves the real functions out of the game's
-- own binary at runtime.
--
-- Set RWG33SDK / RWG34SDK / RWG36SDK to a game's rwsdk directory, or
-- RENDERWARE_ROOT to a directory holding all three.

local GAMES = {
	III = { var = "RWG33SDK", api = "d3d8", version = "3.3", compact = "33" },
	VC  = { var = "RWG34SDK", api = "d3d8", version = "3.4", compact = "34" },
	SA  = { var = "RWG36SDK", api = "d3d9", version = "3.6", compact = "36" },
}

local function env(name)
	local value = os.getenv(name)
	if value == nil or value == "" then
		return nil
	end
	return value
end

-- Layouts an SDK tends to get unpacked into, relative to a search root.
local function candidates(game)
	local root = env("RENDERWARE_ROOT")
	if root == nil then
		return {}
	end

	local list = {}
	for _, layout in ipairs {
		"/RW" .. game.version .. "/Graphics/rwsdk",
		"/RW" .. game.compact .. "/Graphics/rwsdk",
		"/" .. game.version .. "/Graphics/rwsdk",
		"/RenderWare Graphics " .. game.version .. "/rwsdk",
		"/Graphics/rwsdk",
		"/rwsdk",
		"",
	} do
		table.insert(list, root .. layout)
	end
	return list
end

-- Adds the include directory for a game's RenderWare SDK to the current project.
function renderware_includedirs(name)
	local game = GAMES[name]
	if game == nil then
		error("renderware: unknown game " .. tostring(name))
	end

	local sdk = env(game.var)

	if sdk == nil then
		for _, candidate in ipairs(candidates(game)) do
			if os.isfile(candidate .. "/include/" .. game.api .. "/rwcore.h") then
				sdk = candidate
				break
			end
		end
	end

	if sdk == nil then
		error(string.format(
			"RenderWare Graphics %s SDK not found.\n" ..
			"Point %s at its rwsdk directory, or RENDERWARE_ROOT at a directory holding all three.\n" ..
			"Alternatively pass /rwsdk to the create*projects scripts to source the headers\n" ..
			"from plugin-sdk, or --projects= to generate only what you have SDKs for.",
			game.version, game.var))
	end

	local includedir = sdk .. "/include/" .. game.api

	if not os.isfile(includedir .. "/rwcore.h") then
		error(string.format(
			"%s is set to \"%s\", but %s/rwcore.h does not exist.\n" ..
			"It should point at the rwsdk directory of a RenderWare Graphics %s install.",
			game.var, sdk, includedir, game.version))
	end

	includedirs { includedir }
end
