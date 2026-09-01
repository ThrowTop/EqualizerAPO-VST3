@echo off
%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
set "installExitCode=%ERRORLEVEL%"

if not "%installExitCode%"=="0" (
    echo.
    echo EqualizerAPO-VST3 installation did not complete successfully.
    echo Review the message above, then press any key to close this window.
    pause >nul
)

exit /b %installExitCode%
