@echo off
setlocal
pushd "%~dp0"
set "RS_CTEST=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
echo Current suite: ABI/input/geometry/terrain/physics/statistics/config/batch/replay. GUI P4/P5 acceptance remains separate.
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
