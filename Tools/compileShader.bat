@echo on

set "arg1=../assets"
set "arg2=shaders.cfg"
cd %arg1%

for /f "delims=" %%A in (%arg2%) do (
    rem Call the executable with the line as an argument
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\dxc.exe" %%A
)
pause