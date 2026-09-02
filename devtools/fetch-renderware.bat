@echo off
rem ---------------------------------------------------------------------------
rem  fetch-renderware.bat
rem
rem  Assembles a RenderWare header tree from plugin-sdk, so the games can be
rem  built without the original RenderWare Graphics SDKs.
rem
rem  plugin-sdk carries RenderWare headers written for GTA modding, and they
rem  cover everything SilentPatch touches. What comes out of here is only
rem  headers - SilentPatch never links against RenderWare, it resolves the real
rem  functions out of the game's own binary at runtime.
rem
rem  Run directly, or through the /rwsdk switch on the create*projects scripts.
rem
rem  Layout produced, matching what RWG33SDK / RWG34SDK / RWG36SDK expect:
rem      build\renderware\III\include\d3d8\
rem      build\renderware\VC\include\d3d8\
rem      build\renderware\SA\include\d3d9\
rem ---------------------------------------------------------------------------

setlocal

for %%I in ("%~dp0..") do set "ROOT=%%~fI"

set "CLONE=%ROOT%\build\plugin-sdk"
set "DEST=%ROOT%\build\renderware"
set "REPO=https://github.com/DK22Pac/plugin-sdk.git"

rem Already assembled? Nothing to do. Delete build\renderware to force a redo.
if exist "%DEST%\SA\include\d3d9\rwcore.h" (
	echo RenderWare headers already present in %DEST%
	goto :export
)

where git >nul 2>&1
if errorlevel 1 (
	echo.
	echo ERROR: git was not found on PATH, so plugin-sdk cannot be fetched.
	echo.
	exit /b 1
)

if not exist "%CLONE%\.git" (
	echo Fetching plugin-sdk into %CLONE%
	git clone --depth 1 --filter=blob:none --sparse "%REPO%" "%CLONE%"
	if errorlevel 1 (
		echo ERROR: failed to clone plugin-sdk.
		exit /b 1
	)
	git -C "%CLONE%" sparse-checkout set plugin_III/game_III/rw plugin_vc/game_vc/rw plugin_sa/game_sa/rw
	if errorlevel 1 (
		echo ERROR: failed to narrow the plugin-sdk checkout.
		exit /b 1
	)
)

echo Assembling header tree in %DEST%

call :stage III plugin_III\game_III d3d8 || exit /b 1
call :stage VC  plugin_vc\game_vc   d3d8 || exit /b 1
call :stage SA  plugin_sa\game_sa   d3d9 || exit /b 1

rem -----------------------------------------------------------------------
rem  Two fixups. plugin-sdk declares RwEngineInstance as a reference to a
rem  pointer, which is its own idiom for game globals rather than RenderWare's,
rem  and it leaves animKeyFrameSize commented out.
rem
rem  Both edits are verified rather than assumed - if plugin-sdk ever changes
rem  those lines, this stops here instead of failing later as a confusing
rem  compile error.
rem -----------------------------------------------------------------------
call :patch "%DEST%\III\include\d3d8\rwplcore.h" "\*&RwEngineInstance" "*RwEngineInstance" || exit /b 1
call :patch "%DEST%\VC\include\d3d8\rwplcore.h"  "\*&RwEngineInstance" "*RwEngineInstance" || exit /b 1
call :patch "%DEST%\SA\include\d3d9\rwplcore.h"  "\*&RwEngineInstance" "*RwEngineInstance" || exit /b 1
call :patch "%DEST%\SA\include\d3d9\rtanim.h"    "//(\s*RwInt32\s+animKeyFrameSize;)" "$1" || exit /b 1

:export
echo.
echo RenderWare headers ready:
echo     RWG33SDK=%DEST%\III
echo     RWG34SDK=%DEST%\VC
echo     RWG36SDK=%DEST%\SA
echo.
exit /b 0


rem ---------------------------------------------------------------------------
rem  :stage <game dir> <plugin-sdk subdir> <api>
rem ---------------------------------------------------------------------------
:stage
set "SRC=%CLONE%\%~2\rw"
set "OUT=%DEST%\%~1\include\%~3"

if not exist "%SRC%\rwcore.h" (
	echo ERROR: %SRC%\rwcore.h is missing - the plugin-sdk layout has changed.
	exit /b 1
)

if not exist "%OUT%\rw" mkdir "%OUT%\rw" >nul 2>&1
copy /y "%SRC%\*" "%OUT%\" >nul
rem Some headers reach for their error files as rw\<name>.rpe, so they go in twice.
copy /y "%SRC%\*.rpe" "%OUT%\rw\" >nul 2>&1
exit /b 0


rem ---------------------------------------------------------------------------
rem  :patch <file> <regex> <replacement>
rem ---------------------------------------------------------------------------
:patch
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
	"$f = '%~1'; $b = [IO.File]::ReadAllText($f); $a = $b -replace '%~2', '%~3';" ^
	"if ($a -eq $b) { Write-Host ('ERROR: no-op patch on ' + (Split-Path $f -Leaf) + ' - plugin-sdk has changed'); exit 1 };" ^
	"[IO.File]::WriteAllText($f, $a); Write-Host ('  patched ' + (Split-Path $f -Leaf))"
exit /b %ERRORLEVEL%
