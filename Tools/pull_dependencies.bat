@echo off

set "dependencies_path=%~dp0\..\Ragdoll\dependencies"
echo [97mPulling all dependencies[0m

call :pull_dependency "spdlog" "https://github.com/buttception/spdlog.git"
call :pull_dependency "glfw" "https://github.com/buttception/glfw.git"
call :pull_dependency "glm" "https://github.com/buttception/glm.git"
call :pull_dependency "entt" "https://github.com/buttception/entt.git"

set "dependencies_path=%~dp0\..\Editor\dependencies"
call :pull_dependency "imgui" "--branch docking https://github.com/buttception/imgui.git"

call :exit
pause >nul
exit

:pull_dependency
echo [97mCloning %~1[0m
if exist "%dependencies_path%\%~1" (
    echo [93m%~1 exists. Delete the folder if updating is required.[0m
) else (
    git clone %~2 "%dependencies_path%\%~1" 
    if errorlevel 1 (
       echo [91mError pulling %~1 from %~2[0m
    ) else (
       echo [92m%~1 cloned succesfully.[0m
    )
)
goto:eof

:exit
echo [97mScript done[0m
goto:eof