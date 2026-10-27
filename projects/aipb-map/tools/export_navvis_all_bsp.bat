@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "ROOT=%~dp0..\build\debug"
set "CLI=%ROOT%\aipb-map.exe"
set "RESULTS=%ROOT%\results"
set "found=0"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=de_dust2"

if not exist "%CLI%" (
    echo [ERROR] Missing Map Tool: %CLI%
    exit /b 1
)
if not exist "%RESULTS%\" (
    echo [ERROR] No imported map artifacts: %RESULTS%
    exit /b 1
)

for /d %%D in ("%RESULTS%\*") do (
    if /i "%%~nxD"=="%TARGET%" if exist "%%~fD\%%~nxD.aipbmap\manifest.json" (
        set "found=1"
        if exist "%%~fD\%%~nxD.nviz" (
            echo [SKIP] %%~nxD
        ) else (
            echo [EXPORT NAVVIS] %%~nxD
            "%CLI%" export-navvis --input "%%~fD\%%~nxD.aipbmap" --output "%%~fD\%%~nxD.nviz"
            if errorlevel 1 exit /b 1
        )
    )
)

if "%found%"=="0" (
    echo [ERROR] No imported maps found.
    exit /b 1
)
echo [OK] Copy generated *.nviz into cstrike\addons\yapb\data\navvis\
exit /b 0
