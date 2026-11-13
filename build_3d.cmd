@echo off
setlocal EnableExtensions
pushd "%~dp0"
set "RS_CONFIG=Release"
if /I "%~1"=="Debug" set "RS_CONFIG=Debug"
set "RS_GUI=ON"
if /I "%~2"=="--core" set "RS_GUI=OFF"
call scripts3d\vs_cmake.cmd
if not defined RS_CMAKE (
  echo ERROR: cmake.exe not found. Install Visual Studio 2022/2026 with the C++ desktop workload, or set CMAKE_EXE.
  popd
  exit /b 1
)
rem An existing build\3d keeps its generator; a fresh one uses the detected Visual Studio.
set "RS_GEN_ARG="
if not exist build\3d\CMakeCache.txt if defined RS_GENERATOR set RS_GEN_ARG=-G "%RS_GENERATOR%"
"%RS_CMAKE%" -S src3d -B build/3d %RS_GEN_ARG% -A x64 -DRS3D_GUI=%RS_GUI%
if errorlevel 1 goto fail
"%RS_CMAKE%" --build build/3d --config %RS_CONFIG% --parallel -- /nr:false
if errorlevel 1 goto fail
popd
exit /b 0
:fail
popd
exit /b 1
