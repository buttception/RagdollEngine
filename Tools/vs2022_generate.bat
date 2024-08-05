@echo off

set "premake_path=..\premake5.lua"
set "ver=vs2022"

powershell write-host -back White -fore Black Creating solutions
call premake\premake5.exe --file=%premake_path% %ver% 
if errorlevel 1 (
    powershell write-host -back Red -fore Black Premake failed
    pause
    exit
)
powershell write-host -back Green -fore Black Solutions built succesfully
pause