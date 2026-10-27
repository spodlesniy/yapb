@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "ROOT=%~dp0..\build\debug"
set "RESULTS=%ROOT%\results"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=de_dust2"
set "OUTPUT=%RESULTS%\%TARGET%\navigation-issues.md"
set "CSV=%RESULTS%\%TARGET%\navigation-issues.csv"

if not exist "%RESULTS%\%TARGET%\" (
    echo [ERROR] Map results directory not found: %RESULTS%\%TARGET%
    exit /b 1
)

where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0diagnose_navigation.py" --results "%RESULTS%" --map "%TARGET%" --output "%OUTPUT%" --csv "%CSV%"
) else (
    python "%~dp0diagnose_navigation.py" --results "%RESULTS%" --map "%TARGET%" --output "%OUTPUT%" --csv "%CSV%"
)

exit /b %errorlevel%
