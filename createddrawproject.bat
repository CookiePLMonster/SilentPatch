@echo off
rem Generates a solution with just the ddraw proxy.
rem
rem   createddrawproject.bat [action] [/rwsdk] [/open]
rem
rem This is the one project that needs no RenderWare SDK and no vcpkg, so it is
rem the quickest way to check a toolchain is set up correctly.

set "SILENTPATCH_CALLER=%~nx0"
call "%~dp0devtools\createprojects.bat" ddraw %*
exit /b %ERRORLEVEL%
