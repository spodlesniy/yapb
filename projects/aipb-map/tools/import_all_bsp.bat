@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "ROOT=%~dp0..\build\debug"
set "CLI=%ROOT%\aipb-map.exe"
set "MAPS=%ROOT%\maps"
set "RESULTS=%ROOT%\results"
set "failed=0"
set "found=0"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=de_dust2"

if not exist "%CLI%" (
    echo [ERROR] aipb-map.exe not found: %CLI%
    exit /b 1
)
if not exist "%MAPS%\" (
    echo [ERROR] BSP map directory not found: %MAPS%
    exit /b 1
)
if not exist "%RESULTS%\" mkdir "%RESULTS%"
if not exist "%RESULTS%\" (
    echo [ERROR] Cannot create results directory: %RESULTS%
    exit /b 1
)

for /r "%MAPS%" %%F in (*.bsp) do (
  if /i "%%~nF"=="%TARGET%" (
    set "found=1"
    echo.
    echo ===== %%~nxF =====

    if exist "%RESULTS%\%%~nF\%%~nF.aipbmap\manifest.json" (
        echo [SKIP] Already imported: %%~nxF
    ) else (
        if exist "%RESULTS%\%%~nF\%%~nF.aipbmap\" (
            echo [ERROR] Incomplete artifact directory: %%~nF.aipbmap
            set "failed=1"
        ) else (
            if not exist "%RESULTS%\%%~nF\" mkdir "%RESULTS%\%%~nF"
            if not exist "%RESULTS%\%%~nF\" (
                echo [ERROR] Cannot create results directory for %%~nxF
                set "failed=1"
            ) else (
                echo [IMPORT] %%~nxF
                "%CLI%" import --format bsp --input "%%~fF" --output "%RESULTS%\%%~nF\%%~nF.aipbmap" --log-file "%RESULTS%\%%~nF\import.log"
                if errorlevel 1 (
                    echo [ERROR] Import failed: %%~nxF
                    set "failed=1"
                ) else (
                    echo [OK] Imported: %%~nxF
                )
            )
        )
    )
  )
)

if "%found%"=="0" (
    echo [ERROR] No BSP files found in %MAPS%
    exit /b 1
)
endlocal & exit /b %failed%
