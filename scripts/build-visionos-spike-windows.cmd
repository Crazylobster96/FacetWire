@echo off
setlocal EnableExtensions

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Installer vswhere.exe was not found.
  exit /b 1
)

for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FW_VS=%%I"
if not defined FW_VS (
  echo Visual Studio with Desktop development with C++ was not found.
  exit /b 1
)

call "%FW_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%

set "FW_CMAKE=%FW_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "FW_NINJA=%FW_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "FW_BUILD=build\visionos-windows-spike"

"%FW_CMAKE%" -S spikes\visionos_host\windows -B "%FW_BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%FW_NINJA%"
if errorlevel 1 exit /b %errorlevel%
"%FW_CMAKE%" --build "%FW_BUILD%"
if errorlevel 1 exit /b %errorlevel%
"%FW_CMAKE%" --build "%FW_BUILD%" --target test
if errorlevel 1 exit /b %errorlevel%

set "FW_EXE=%CD%\%FW_BUILD%\facetwire_visionos_windows_spike.exe"
echo.
echo Windows visionOS contract simulator ready:
echo   %FW_EXE%
if /I "%~1"=="run" start "FacetWire visionOS Spike" "%FW_EXE%"
exit /b 0
