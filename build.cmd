@echo off
setlocal
call "%~dp0tools\msvc-env.cmd"
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /W4 /O2 /MT /LD src\plugin.cpp src\game_adapter.cpp /Fo:build\ /Fe:build\DivaMania6K.dll /link d3d11.lib dxgi.lib d3dcompiler.lib user32.lib /IMPLIB:build\DivaMania6K.lib
exit /b %errorlevel%
