@echo off
setlocal
set "MSYSTEM=UCRT64"
set "CHERE_INVOKING=1"
set "WUWAOS_BASH=%~dp0build\portable\msys64\usr\bin\bash.exe"
if not exist "%WUWAOS_BASH%" (
  echo Portable environment not found. Extract the complete Windows portable release first.
  exit /b 1
)
pushd "%~dp0"
if errorlevel 1 exit /b 1
"%WUWAOS_BASH%" --noprofile --norc tools/portable-entry.sh "%~1"
set "WUWAOS_EXIT=%ERRORLEVEL%"
popd
exit /b %WUWAOS_EXIT%
