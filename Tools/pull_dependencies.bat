@echo off

set "dependencies_path=%~dp0\..\Ragdoll\dependencies"
echo [97mPulling all dependencies[0m

call :pull_dependency "spdlog" "https://github.com/buttception/spdlog.git"
call :pull_dependency "glfw" "https://github.com/buttception/glfw.git"
call :pull_dependency "entt" "https://github.com/buttception/entt.git"
call :pull_dependency "tinygltf" "https://github.com/syoyo/tinygltf.git"
call :pull_dependency "microprofile" "https://github.com/jonasmr/microprofile.git"
call :pull_dependency "imgui" "--branch docking https://github.com/buttception/imgui.git"
call :pull_dependency "cxxopts" "https://github.com/jarro2783/cxxopts.git"
call :pull_dependency "taskflow" "https://github.com/taskflow/taskflow.git"
call :pull_dependency "dlss" "https://github.com/NVIDIA/DLSS.git"
call :pull_dependency "directxtex" "https://github.com/microsoft/DirectXTex.git"

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
    git -C "%dependencies_path%\%~1" submodule update --init --recursive
    if errorlevel 1 (
        echo [91mError updating submodules %~1 from %~2[0m
    ) else (
        echo [92m%~1 submodules updated succesfully.[0m
    )
)
goto:eof

:exit
echo [97mScript done[0m
goto:eof