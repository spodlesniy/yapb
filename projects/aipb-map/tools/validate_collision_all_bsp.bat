@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "ROOT=%~dp0..\build\debug"
set "CLI=%ROOT%\aipb-map.exe"
set "RESULTS=%ROOT%\results"
set "failed=0"
set "found=0"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=de_dust2"

if not exist "%CLI%" (
    echo [ERROR] aipb-map.exe not found: %CLI%
    exit /b 1
)
if not exist "%RESULTS%\" (
    echo [ERROR] Results directory not found: %RESULTS%
    exit /b 1
)

for /d %%D in ("%RESULTS%\*") do (
    if /i "%%~nxD"=="%TARGET%" if exist "%%~fD\%%~nxD.aipbmap\manifest.json" (
        set "found=1"
        echo.
        echo ===== %%~nxD =====

        if exist "%%~fD\collision-validation.json" (
            echo [SKIP] Collision validation report exists: %%~nxD
        ) else (
            echo [VALIDATE COLLISION] %%~nxD
            "%CLI%" validate-collision --input "%%~fD\%%~nxD.aipbmap" --output "%%~fD\collision-validation.json" --log-file "%%~fD\collision-validation.log"
            if errorlevel 1 (
                echo [ERROR] Collision validation failed: %%~nxD
                set "failed=1"
            ) else (
                echo [OK] Collision validated: %%~nxD
            )
        )
    )
)

if "%found%"=="0" (
    echo [ERROR] No imported map artifacts found in %RESULTS%
    exit /b 1
)
endlocal & exit /b %failed%
