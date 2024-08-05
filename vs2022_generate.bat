@echo off

set "ver=vs2022"

powershell write-host -back White -fore Black Creating solutions
call Tools\premake\premake5.exe %ver%
IF errorlevel 1 (
    powershell write-host -back Red -fore Black Premake failed
    PAUSE
    exit
)
powershell write-host -back Green -fore Black Solutions built succesfully
PAUSE