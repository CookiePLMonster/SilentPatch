@echo off
rem Generates a solution with the three game projects, leaving out the ddraw proxy.
rem
rem   creategameprojects.bat [action] [/rwsdk] [/open]
rem
rem Pass /rwsdk to fetch RenderWare headers from plugin-sdk rather than
rem supplying the original SDKs yourself.

set "SILENTPATCH_CALLER=%~nx0"
call "%~dp0devtools\createprojects.bat" games %*
exit /b %ERRORLEVEL%
