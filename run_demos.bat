@echo off
setlocal enabledelayedexpansion

:: ==================================================
:: CONFIGURATION
:: ==================================================
set "BUILD_DIR=build"
set "DEMOS=bubbles deepsea fractal neon polar raymarch ripple starfield tzozen voronoi warp"

:: ==================================================
:: VALIDATION
:: ==================================================
if not exist "%BUILD_DIR%" (
    echo [ERROR] Build directory '%BUILD_DIR%' not found.
    pause
    exit /b 1
)

echo ==================================================
echo    Starting ESHI Demo Suite
echo ==================================================

:: ==================================================
:: EXECUTION LOOP
:: ==================================================
for %%d in (%DEMOS%) do (
    :: Check if file exists directly using the variable path
    if exist "%BUILD_DIR%\%%d.exe" (
        echo.
        echo ------------------------------------------
        echo  [RUNNING] %%d
        echo ------------------------------------------
        
        :: Run the executable
        "%BUILD_DIR%\%%d.exe"
        
        echo  [DONE] %%d finished.
    ) else (
        echo.
        echo  [SKIPPING] %%d - Executable not found.
    )
)

echo.
echo ==================================================
echo    All demos completed.
echo ==================================================
pause
