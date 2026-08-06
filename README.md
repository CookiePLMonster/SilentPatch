# SilentPatch

<p align="center">
  <img src="https://i.imgur.com/sCDzq12.png" alt="Logo">
</p>

SilentPatch for the 3D-era Grand Theft Auto games is the first and flagship release of the "SilentPatch family", providing numerous fixes for this beloved franchise.
SilentPatch addresses a wide range of issues, from critical fixes for crashes and other blockers to various major and minor improvements identified by
the passionate community in these games over decades. SilentPatch does not alter the core gameplay experience, making it an optimal choice
for both first-time players and the old guard returning for yet another playthrough.

## Featured fixes

* [Fixes in GTA III](CHANGELOG-III.md)
* [Fixes in GTA Vice City](CHANGELOG-VC.md)
* [Fixes in GTA San Andreas](CHANGELOG-SA.md)

## Compilation requirements

* Visual Studio 2017 or newer with `C++ Windows XP Support for VS 2017 (v141) tools` installed. Newer toolsets will work too, but the projects will require retargeting.
* [premake5](https://premake.github.io/download), on `PATH`.
* [vcpkg](https://vcpkg.io/) installed separately or as a Visual Studio component. Necessary for SP for San Andreas to include `libflac`.
* RenderWare Graphics SDK. Each game requires their corresponding RenderWare version and an environment variable pointing at the `RW3.x\Graphics\rwsdk` directory:
  * GTA III: RW 3.3, D3D8, `RWG33SDK` variable.
  * GTA Vice City: RW 3.4, D3D8, `RWG34SDK` variable.
  * GTA San Andreas: RW 3.6, D3D9, `RWG36SDK` variable.

  Set `RENDERWARE_ROOT` instead if all three live under one directory, or pass `/rwsdk` below to source the headers from
  [plugin-sdk](https://github.com/DK22Pac/plugin-sdk).

## Building

```
git submodule update --init --recursive
createallprojects.bat
```

| Script | Projects |
| --- | --- |
| `createallprojects.bat` | all three games plus the ddraw proxy |
| `creategameprojects.bat` | the three games |
| `createddrawproject.bat` | just the ddraw proxy — needs no RenderWare SDK and no vcpkg |

Each takes an optional premake action (`vs2022` by default) and the switches `/rwsdk` and `/open`, in any order. The
v141_xp toolset is selected when it is installed, and the default toolset otherwise, with a warning that the result will
not load on Windows XP.

The solution is written to `build\SilentPatch.sln`; the `SilentPatch.sln` in the root is the MSBuild one and is left
alone. `premake5` can also be run directly — `premake5 --projects=sa vs2022` generates San Andreas on its own.

## Contribution guidelines

* Contributions with bug fixes are welcome, but you must be able to explain why you believe they fix a bug, rather than alter a design decision that doesn't suit you.
  I reserve the right to reject submissions that cannot be unambiguously classified as fixes.
* Contributions for GTA III and Vice City must use patterns and support all game versions. For GTA San Andreas, contributions must support version 1.0, but preferably
  also the new binaries (newsteam/RGL).
* This repository is not intended for game support. Issues such as "I installed mods and the game now crashes" will be closed.

## Credits

SilentPatch includes code contributions from:
* aap
* B1ack_Wh1te
* CanerKaraca
* DK22Pac
* Fire_Head
* iFarbod
* Kaizo M
* Nick007J
* NTAuthority
* rx
* Sergeanur
* spaceeinstein
* Wesser
