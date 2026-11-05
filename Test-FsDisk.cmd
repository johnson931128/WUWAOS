@echo off
setlocal
set "MSYSTEM=UCRT64"
set "WUWAOS_PYTHON=%~dp0build\portable\msys64\ucrt64\bin\python.exe"
if not exist "%WUWAOS_PYTHON%" (
  echo Portable environment not found. Extract the complete Windows portable release first.
  exit /b 1
)
"%WUWAOS_PYTHON%" -B "%~dp0tools\test_fs_disk.py" %*
exit /b %ERRORLEVEL%
