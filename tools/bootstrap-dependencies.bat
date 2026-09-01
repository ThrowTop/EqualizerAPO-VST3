@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0bootstrap-dependencies.ps1"
set "EAPO_EXIT_CODE=%ERRORLEVEL%"
if not "%EAPO_EXIT_CODE%"=="0" pause
exit /b %EAPO_EXIT_CODE%
