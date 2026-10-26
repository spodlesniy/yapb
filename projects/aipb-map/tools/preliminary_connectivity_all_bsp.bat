@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "ROOT=%~dp0..\build\debug"
set "CLI=%ROOT%\aipb-map.exe"
set "RESULTS=%ROOT%\results"
set "failed=0"
set "found=0"

if not exist "%CLI%" (
    echo [ERROR] aipb-map.exe not found: %CLI%
    exit /b 1
)
if not exist "%RESULTS%\" (
    echo [ERROR] Results directory not found: %RESULTS%
    exit /b 1
)

for /d %%D in ("%RESULTS%\*") do (
    if exist "%%~fD\%%~nxD.aipbmap\manifest.json" (
        set "found=1"
        echo.
        echo ===== %%~nxD =====

        if exist "%%~fD\preliminary-connectivity.json" (
            echo [SKIP] Connectivity report exists: %%~nxD
        ) else (
            echo [PRELIMINARY CONNECTIVITY] %%~nxD
            "%CLI%" preliminary-connectivity --input "%%~fD\%%~nxD.aipbmap" --output "%%~fD\preliminary-connectivity.json"
            if errorlevel 1 (
                echo [ERROR] Connectivity failed: %%~nxD
                set "failed=1"
            ) else (
                echo [OK] Connectivity generated: %%~nxD
            )
        )
    )
)

if "%found%"=="0" (
    echo [ERROR] No imported map artifacts found in %RESULTS%
    exit /b 1
)
endlocal & exit /b %failed%
