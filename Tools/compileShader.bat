@echo on

set "arg1=../assets"
set "arg2=shaders.cfg"
cd %arg1%

for /f "delims=" %%A in (%arg2%) do (
    rem Call the executable with the line as an argument
    "dxc.exe" %%A
)
pause