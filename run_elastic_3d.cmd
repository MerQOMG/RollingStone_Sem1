@echo off
call "%~dp0run_3d.cmd" --config "%~dp0configs3d\parameterized_elastic.ini" %*
exit /b %errorlevel%
