@echo off
setlocal EnableExtensions
pushd "%~dp0"
set "RS_CONFIG=Release"
if /I "%~1"=="Debug" set "RS_CONFIG=Debug"
set "RS_GUI=ON"
if /I "%~2"=="--core" set "RS_GUI=OFF"
set "RS_CMAKE=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if defined CMAKE_EXE set "RS_CMAKE=%CMAKE_EXE%"
if not exist "%RS_CMAKE%" (
  echo ERROR: Set CMAKE_EXE to your existing cmake.exe.
  popd
  exit /b 1
)
"%RS_CMAKE%" -S src3d -B build/3d -G "Visual Studio 17 2022" -A x64 -T v143 -DRS3D_GUI=%RS_GUI%
if errorlevel 1 goto fail
"%RS_CMAKE%" --build build/3d --config %RS_CONFIG% --parallel 1 -- /nr:false
if errorlevel 1 goto fail
popd
exit /b 0
:fail
popd
exit /b 1

