@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "WIN_SDK=C:\Program Files (x86)\Windows Kits\10"
set "WIN_VER=10.0.26100.0"
set "PATH=C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;%PATH%"
set "PATH=%WIN_SDK%\bin\%WIN_VER%\x64;%PATH%"
set "LIB=%WIN_SDK%\Lib\%WIN_VER%\um\x64;%WIN_SDK%\Lib\%WIN_VER%\ucrt\x64;%LIB%"
set "INCLUDE=%WIN_SDK%\Include\%WIN_VER%\um;%WIN_SDK%\Include\%WIN_VER%\ucrt;%WIN_SDK%\Include\%WIN_VER%\shared;%INCLUDE%"
cmake --build "%~dp0build"
endlocal
