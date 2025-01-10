@echo off
setlocal EnableDelayedExpansion

set CPPDIR=c:/cpp
set WORKSPACE=%CPPDIR%/workspace

set LIBYAML_PATH=%WORKSPACE%/yaml-0.2.5
set DMALLOC_PATH=%WORKSPACE%/dmalloc-5.6.5

call :appendPath %CPPDIR%/msys64/ucrt64/bin
call :appendPath %CPPDIR%/msys64/usr/bin
call :appendPath %JAVADIR%/git/bin
call :appendPath %CPPDIR%/VSCode


rem ./dmalloc -f dmallocrc -l ./dmalloc.log -i 100 log
set DMALLOC_OPTIONS=debug=0x4440d2b,inter=100,log=%DTP_HOME%/logs/dmalloc.log

START code.exe --preserve-env .

exit /B %ERRORLEVEL%

:appendPath
for %%a in (%*) do (
   if "!path:%%~a=!" equ "!path!" set "path=!path!;%%a"
)
goto :eof

