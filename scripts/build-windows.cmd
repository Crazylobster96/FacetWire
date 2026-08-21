@echo off
setlocal EnableExtensions

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Installer vswhere.exe was not found.
  exit /b 1
)

for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FW_VS=%%I"
if not defined FW_VS (
  echo Visual Studio with the C++ toolchain was not found.
  exit /b 1
)

call "%FW_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%

set "FW_CMAKE=%FW_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "FW_NINJA=%FW_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "FW_SHARED=OFF"
if /I "%~1"=="shared" set "FW_SHARED=ON"

if not exist "%FW_CMAKE%" (
  echo Visual Studio bundled CMake was not found.
  exit /b 1
)
if not exist "%FW_NINJA%" (
  echo Visual Studio bundled Ninja was not found.
  exit /b 1
)

"%FW_CMAKE%" -S . -B build\ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFACETWIRE_BUILD_TESTS=ON -DFACETWIRE_BUILD_SHARED=%FW_SHARED% -DCMAKE_MAKE_PROGRAM="%FW_NINJA%"
if errorlevel 1 exit /b %errorlevel%

"%FW_CMAKE%" --build build\ninja
if errorlevel 1 exit /b %errorlevel%

"%FW_CMAKE%" --build build\ninja --target test
exit /b %errorlevel%
