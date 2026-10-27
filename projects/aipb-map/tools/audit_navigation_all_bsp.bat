@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "ROOT=%~dp0..\build\debug"
set "RESULTS=%ROOT%\results"
set "OUTPUT=%RESULTS%\navigation-readiness.md"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=de_dust2"

if not exist "%RESULTS%\" (
    echo [ERROR] Results directory not found: %RESULTS%
    exit /b 1
)

where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0audit_navigation.py" --results "%RESULTS%" --output "%OUTPUT%" --map "%TARGET%"
) else (
    python "%~dp0audit_navigation.py" --results "%RESULTS%" --output "%OUTPUT%"
)

exit /b %errorlevel%
