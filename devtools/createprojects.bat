@echo off
rem ---------------------------------------------------------------------------
rem  createprojects.bat
rem
rem  Shared implementation behind the create*projects.bat scripts in the root.
rem  Generates a Visual Studio solution with premake.
rem
rem  Usage: createprojects.bat <projects> [action] [/rwsdk] [/open]
rem
rem    action  premake action, e.g. vs2022 (default) or vs2019.
rem    /rwsdk  provision RenderWare headers from plugin-sdk instead of using
rem            the original SDKs. See devtools\fetch-renderware.bat.
rem    /open   open the generated solution when done.
rem
rem  The generated solution lands in build\, NOT in the root - the
rem  SilentPatch.sln checked into git is the MSBuild one and is left alone.
rem ---------------------------------------------------------------------------

setlocal

rem Captured before the argument loop below: a bare "shift" moves %0 along with
rem everything else, so %~dp0 stops pointing at this script once parsing starts.
set "SCRIPTDIR=%~dp0"
for %%I in ("%~dp0..") do set "ROOT=%%~fI"

set "PROJECTS=%~1"
if "%PROJECTS%"=="" set "PROJECTS=all"

rem Everything after the project set is optional and order-independent:
rem switches start with /, and the first bare word is the premake action.
set "ACTION="
set "OPEN="
set "FETCHRW="

rem shift /2 leaves %0 and the project set in %1 alone.
:parse
if "%~2"=="" goto :parsed
if /i "%~2"=="/open"  ( set "OPEN=1"    & shift /2 & goto :parse )
if /i "%~2"=="/rwsdk" ( set "FETCHRW=1" & shift /2 & goto :parse )
if not defined ACTION ( set "ACTION=%~2" & shift /2 & goto :parse )
echo ERROR: unrecognised argument "%~2"
exit /b 1
:parsed

if "%ACTION%"=="" set "ACTION=%SILENTPATCH_ACTION%"
if "%ACTION%"=="" set "ACTION=vs2022"

rem The root wrappers announce themselves so error messages can name the script
rem that was actually run rather than this one.
set "CALLER=%SILENTPATCH_CALLER%"
if "%CALLER%"=="" set "CALLER=%~nx0 %PROJECTS%"

where premake5 >nul 2>&1
if errorlevel 1 (
	echo.
	echo ERROR: premake5 was not found on PATH.
	echo        Get it from https://premake.github.io/download and put it on PATH.
	echo.
	exit /b 1
)

rem Releases are built with the XP toolset, so use it when it is installed and
rem fall back to the default toolset otherwise. Microsoft.VisualStudio.Component.WinXP
rem is the component that carries both v141_xp and the Windows 7.0 SDK.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "XPTOOLSET="
set "TOOLSETARG="
if exist "%VSWHERE%" (
	for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.WinXP -property installationPath 2^>nul`) do set "XPTOOLSET=%%I"
)
if not defined XPTOOLSET set "TOOLSETARG=--modern-toolset"

rem Optional: provision RenderWare headers from plugin-sdk instead of requiring
rem the original SDKs. Off unless /rwsdk is passed.
if defined FETCHRW (
	call "%SCRIPTDIR%fetch-renderware.bat"
	if errorlevel 1 exit /b 1
	set "RWG33SDK=%ROOT%\build\renderware\III"
	set "RWG34SDK=%ROOT%\build\renderware\VC"
	set "RWG36SDK=%ROOT%\build\renderware\SA"
)

echo.
echo Generating SilentPatch projects
echo     projects : %PROJECTS%
echo     action   : %ACTION%
if defined FETCHRW echo     rwsdk    : fetched from plugin-sdk
if defined TOOLSETARG (
	echo.
	echo     NOTE: the v141_xp toolset is not installed, so the default toolset
	echo           was selected. The modules will not load on Windows XP.
	echo           Install "C++ Windows XP Support for VS 2017 [v141] tools"
	echo           to build them the way releases are built.
)
echo.

pushd "%ROOT%"
premake5 --projects=%PROJECTS% %TOOLSETARG% %ACTION%
set "RESULT=%ERRORLEVEL%"
popd

if not "%RESULT%"=="0" (
	echo.
	echo Project generation FAILED.
	echo.
	exit /b %RESULT%
)

set "SLN=%ROOT%\build\SilentPatch.sln"

echo.
echo Done. Solution written to:
echo     %SLN%
echo.

if defined OPEN (
	echo Opening solution...
	start "" "%SLN%"
)

exit /b 0
