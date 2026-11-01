@echo off
setlocal
call "%~dp0tools\msvc-env.cmd"
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /W4 /O2 /MT /DDIVAMANIA_TEST /LD src\plugin.cpp src\game_adapter.cpp /Fo:build\ /Fe:build\DivaMania6K-test.dll /link d3d11.lib dxgi.lib d3dcompiler.lib user32.lib /IMPLIB:build\DivaMania6K-test.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /O2 /MT /DDIVAMANIA_TEST src\adapter_test.cpp src\game_adapter.cpp /Fo:build\ /Fe:build\adapter_test.exe
if errorlevel 1 exit /b 1
build\adapter_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /O2 /MT src\smoke_test.cpp /Fo:build\smoke_test.obj /Fe:build\smoke_test.exe /link d3d11.lib dxgi.lib user32.lib
if errorlevel 1 exit /b 1
build\smoke_test.exe
if errorlevel 1 exit /b 1
build\smoke_test.exe singlethreaded
exit /b %errorlevel%
