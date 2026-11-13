@echo off
setlocal
pushd "%~dp0"
call scripts3d\vs_cmake.cmd
if not defined RS_CTEST (
  echo ERROR: ctest.exe not found. Install Visual Studio 2022/2026 with the C++ desktop workload, or set CTEST_EXE.
  popd
  exit /b 1
)
echo Current suite: 21 ABI/input/geometry/physics/barrier/obstacle/analysis/archive/playback/UI-model tests. Native GUI acceptance remains separate.
"%RS_CTEST%" --test-dir build/3d -C Release --output-on-failure
set "RS_EXIT=%errorlevel%"
if not "%RS_EXIT%"=="0" goto done
if /I "%~1"=="--geometry-full" (
  build\3d\bin\rs3d_geometry_tests.exe --full results3d\geometry_acceptance
  if errorlevel 1 set "RS_EXIT=1"
)
:done
popd
exit /b %RS_EXIT%
