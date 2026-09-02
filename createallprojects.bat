@echo off
rem Generates a solution with every project: all three games plus the ddraw proxy.
rem
rem   createallprojects.bat [action] [/rwsdk] [/open]
rem
rem Needs all three RenderWare SDKs. Pass /rwsdk to fetch headers from
rem plugin-sdk instead, or use creategameprojects.bat / createddrawproject.bat
rem if you only have some of them.

set "SILENTPATCH_CALLER=%~nx0"
call "%~dp0devtools\createprojects.bat" all %*
exit /b %ERRORLEVEL%
