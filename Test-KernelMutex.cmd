@echo off
call "%~dp0Start-WUWAOS.cmd" test-kmutex
exit /b %ERRORLEVEL%
