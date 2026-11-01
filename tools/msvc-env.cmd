@echo off
if defined VSCMD_VER exit /b 0
set "DIVA_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%DIVA_VSWHERE%" (
  echo Visual Studio C++ Build Tools and Windows SDK are required.
  exit /b 1
)
set "DIVA_VS_PATH="
for /f "usebackq tokens=*" %%i in (`"%DIVA_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DIVA_VS_PATH=%%i"
if not defined DIVA_VS_PATH (
  echo No Visual Studio C++ x64 toolchain found.
  exit /b 1
)
call "%DIVA_VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
exit /b %errorlevel%
