@echo off
setlocal enableextensions disabledelayedexpansion

set "premake_path=..\premake5.lua"
set "ver=vs2022"

echo [97mCreating solution[0m

call premake\premake5.exe --file=%premake_path% %ver% 
if errorlevel 1 (
    echo [91mPremake failed[0m
    pause
    exit
)
echo [92mSolution built successfully[0m
pause