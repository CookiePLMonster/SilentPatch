-- Each project carries its module name, revision and copyright range in a
-- versionmeta.props sheet, which SilentPatch.rc turns into the VS_VERSION_INFO
-- block. Those sheets are read here rather than duplicated, so a revision bump
-- is still made in one place and both build systems pick it up.

local KEYS = {
	"SILENTPATCH_NAME",
	"SILENTPATCH_EXT",
	"SILENTPATCH_FULL_NAME",
	"SILENTPATCH_REVISION_ID",
	"SILENTPATCH_BUILD_ID",
	"SILENTPATCH_COPYRIGHT",
}

local function trim(s)
	return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

-- Reads <dir>/versionmeta.props and returns the values as a table.
function read_version_meta(dir)
	local path = dir .. "/versionmeta.props"

	local file = io.open(path, "r")
	if not file then
		error("versionmeta: cannot open " .. path)
	end
	local xml = file:read("*a")
	file:close()

	local meta = {}
	for _, key in ipairs(KEYS) do
		local value = xml:match("<" .. key .. ">(.-)</" .. key .. ">")
		if not value then
			error("versionmeta: " .. path .. " does not define " .. key)
		end
		meta[key] = trim(value)
	end
	return meta
end

-- Applies a project's version metadata as preprocessor definitions, and hands
-- back the table so the caller can reuse the name and extension. The values are
-- deliberately unquoted: SilentPatch.rc stringizes them itself.
function version_info(dir)
	local meta = read_version_meta(dir)

	local list = {}
	for _, key in ipairs(KEYS) do
		table.insert(list, key .. "=" .. meta[key])
	end
	defines(list)

	return meta
end
