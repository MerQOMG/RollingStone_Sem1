@echo off
call "%~dp0run_3d.cmd" --config "%~dp0configs3d\obstacles_heatmap_timeline.ini" %*
exit /b %errorlevel%
