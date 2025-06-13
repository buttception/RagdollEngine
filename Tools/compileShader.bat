@echo off
setlocal enabledelayedexpansion

set "arg1=../assets"
set "arg2=shaders.cfg"
cd %arg1%

set "FOLDER=../assets/cso"
if not exist "%FOLDER%" (
    mkdir "%FOLDER%"
    echo Created folder: %FOLDER%
) else (
    echo Folder already exists: %FOLDER%
)

set "KITS_PATH=%ProgramFiles(x86)%\Windows Kits\10\bin"
set "DXC_EXE="
set "LATEST_VER=0"

rem Loop through SDK versions
for /d %%D in ("%KITS_PATH%\*") do (
    set "VER=%%~nxD"
    set "VER_STR=!VER:.=!"
    if !VER_STR! GTR !LATEST_VER! (
        if exist "%%~fD\x64\dxc.exe" (
            set "LATEST_VER=!VER_STR!"
            set "DXC_EXE=%%~fD\x64\dxc.exe"
        )
    )
)

if defined DXC_EXE (
    echo Found dxc.exe at:
    echo !DXC_EXE! 
) else (
    echo ERROR: dxc.exe not found in !KITS_PATH! 
)

rem Only compile if DXC was found
@echo on
if defined DXC_EXE (
    for /f "delims=" %%A in (%arg2%) do (
        !DXC_EXE! %%A
    )
)
@echo off

pause
exit /b
