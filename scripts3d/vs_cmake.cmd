@echo off
rem Finds cmake/ctest and a Visual Studio generator for build_3d.cmd and test_3d.cmd.
rem Sets RS_CMAKE, RS_CTEST and RS_GENERATOR (empty = let CMake choose). Honours CMAKE_EXE and CTEST_EXE.
set "RS_CMAKE="
set "RS_CTEST="
set "RS_GENERATOR="
set "RS_VSROOT="
set "RS_VSVER="
if defined CMAKE_EXE set "RS_CMAKE=%CMAKE_EXE%"
if defined CTEST_EXE set "RS_CTEST=%CTEST_EXE%"
set "RS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%RS_VSWHERE%" goto path_fallback
for /f "usebackq delims=" %%i in (`"%RS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "RS_VSROOT=%%i"
for /f "usebackq delims=" %%i in (`"%RS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion`) do set "RS_VSVER=%%i"
if not defined RS_VSROOT goto path_fallback
if not defined RS_CMAKE if exist "%RS_VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "RS_CMAKE=%RS_VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined RS_CTEST if exist "%RS_VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" set "RS_CTEST=%RS_VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if "%RS_VSVER:~0,3%"=="17." set "RS_GENERATOR=Visual Studio 17 2022"
if "%RS_VSVER:~0,3%"=="18." set "RS_GENERATOR=Visual Studio 18 2026"
:path_fallback
if not defined RS_CMAKE for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined RS_CMAKE set "RS_CMAKE=%%i"
if not defined RS_CTEST for /f "delims=" %%i in ('where ctest 2^>nul') do if not defined RS_CTEST set "RS_CTEST=%%i"
exit /b 0
