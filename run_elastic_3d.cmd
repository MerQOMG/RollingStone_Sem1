@echo off
call "%~dp0run_3d.cmd" --config "%~dp0configs3d\elastic_barrier.ini" %*
exit /b %errorlevel%
