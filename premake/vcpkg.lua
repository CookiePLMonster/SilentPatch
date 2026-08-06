-- Adds the vcpkg properties Visual Studio understands to the generated
-- projects. SilentPatchSA needs VcpkgEnableManifest and VcpkgUseStatic to pick
-- up libflac from its vcpkg.json.

require('vstudio')

premake.api.register {
	name = "vcpkg",
	scope = "config",
	kind = "boolean"
}
premake.api.register {
	name = "vcpkgmanifest",
	scope = "config",
	kind = "boolean"
}
premake.api.register {
	name = "vcpkgstatic",
	scope = "config",
	kind = "boolean"
}
premake.api.register {
	name = "vcpkgconfig",
	scope = "config",
	kind = "string"
}
-- vcpkg looks for vcpkg.json in directories above the project file. Generated
-- projects do not sit next to the manifest, so the root is given explicitly.
premake.api.register {
	name = "vcpkgmanifestroot",
	scope = "config",
	kind = "string"
}

premake.override(premake.vstudio.vc2010.elements, "configurationProperties", function(base, cfg)
	local calls = base(cfg)
	table.insert(calls, function(cfg)
		if cfg.vcpkg ~= nil then
			premake.w('<VcpkgEnabled>%s</VcpkgEnabled>', cfg.vcpkg)
		end
		if cfg.vcpkgmanifest ~= nil then
			premake.w('<VcpkgEnableManifest>%s</VcpkgEnableManifest>', cfg.vcpkgmanifest)
		end
		if cfg.vcpkgstatic ~= nil then
			premake.w('<VcpkgUseStatic>%s</VcpkgUseStatic>', cfg.vcpkgstatic)
		end
		if cfg.vcpkgconfig ~= nil then
			premake.w('<VcpkgConfiguration>%s</VcpkgConfiguration>', cfg.vcpkgconfig)
		end
		if cfg.vcpkgmanifestroot ~= nil then
			premake.w('<VcpkgManifestRoot>%s</VcpkgManifestRoot>', cfg.vcpkgmanifestroot)
		end
	end)
	return calls
end)
