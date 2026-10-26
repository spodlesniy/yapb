
@echo off
setlocal EnableExtensions

set "ROOT=%~dp0\..\build\debug"
set "CLI=%ROOT%\aipb-map.exe"
set "RESULTS=%ROOT%\results"
set "MAPS=%ROOT%\maps"

if not exist "%CLI%" (
    echo [ERROR] aipb-map.exe not found.
    exit /b 1
)

if not exist "%RESULTS%" mkdir "%RESULTS%"

for /r "%MAPS%" %%F in (*.bsp) do (
    echo.
    echo ===== %%~nxF =====

    if exist "%RESULTS%\%%~nF\" (
        echo [SKIP] Folder already exists: %RESULTS%\%%~nF
    ) else (
        mkdir "%RESULTS%\%%~nF"

        echo [IMPORT] %%~nxF
        "%CLI%" import --format bsp --input "%%~fF" --output "%RESULTS%\%%~nF\%%~nF.aipbmap" --log-file "%RESULTS%\%%~nF\import.log"

        if errorlevel 1 (
            echo [ERROR] Import failed: %%~nxF
        ) else (
            echo [INSPECT] %%~nxF
            "%CLI%" inspect --input "%RESULTS%\%%~nF\%%~nF.aipbmap" --output "%RESULTS%\%%~nF\inspection.json" --log-file "%RESULTS%\%%~nF\inspect.log"

            if errorlevel 1 (
                echo [ERROR] Inspect failed: %%~nxF
            ) else (
                echo [OK] %%~nxF
            )
        )
    )
)

echo.
echo All maps processed.
endlocal
