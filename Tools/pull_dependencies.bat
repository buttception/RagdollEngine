@echo off

set "dependencies_path=%~dp0\..\Ragdoll\dependencies"
powershell write-host -back White -fore Black Pulling all dependencies

call :pull_dependency "spdlog" "https://github.com/buttception/spdlog.git"
call :exit
pause >nul
exit

:pull_dependency
powershell write-host -back White -fore Black Cloning %~1...
if exist "%dependencies_path%\%~1" (
    powershell write-host -back Yellow -fore Black %~1 exists. Delete the folder if updating is required.
) else (
    git clone %~2 "%dependencies_path%\%~1" 
    if errorlevel 1 (
       powershell write-host -back Red -fore Black Error pulling %~1 from %~2
    ) else (
       powershell write-host -back Green -fore Black %~1 cloned succesfully.
    )
)
goto:eof

:exit
powershell write-host -back White -fore Black Script done
goto:eof