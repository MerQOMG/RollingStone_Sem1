@echo off
setlocal
pushd "%~dp0"
if not exist "build\3d\bin\RollingStone3D.exe" (
  echo ERROR: Run build_3d.cmd first.
  popd
  exit /b 1
)
if "%~1"=="" (
  "build\3d\bin\RollingStone3D.exe"
) else (
  "build\3d\bin\RollingStone3D.exe" %*
)
set "RS_EXIT=%errorlevel%"
popd
exit /b %RS_EXIT%
