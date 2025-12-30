@echo off
setlocal enabledelayedexpansion

:: --- CONFIGURATION ---
set VCPKG_ROOT=C:\Users\lepus\git\eshi\vcpkg_installed

:: Compiler Flags
set CL_INCLUDES=/I. /I"%VCPKG_ROOT%\arm64-windows\include"

:: Linker Libs
set LIBS=/LIBPATH:"%VCPKG_ROOT%\arm64-windows\lib" SDL2.lib avcodec.lib avformat.lib avutil.lib swscale.lib opengl32.lib

:: Clean build directory
if exist build rmdir /s /q build
mkdir build

:: --- BUILD 1: The Main App (shader.cpp) ---
echo.
echo [1/2] Building Default: eshi.exe ...

:: SHADER_PATH is "../shader.cpp" (relative to build dir)
cl /nologo /openmp /O2 /EHsc /std:c++14 /MD /D_USE_MATH_DEFINES /DSDL_MAIN_HANDLED /DLINK_SHADER /DUSE_OPENGL /DSHADER_PATH=\"../shader.cpp\" %CL_INCLUDES% /Febuild\eshi.exe main.cpp shader.cpp /link %LIBS% shell32.lib user32.lib gdi32.lib
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- BUILD 2: The Examples (examples/*.cpp) ---
echo.
echo [2/2] Building Examples...

for %%f in (examples\*.cpp) do (
    set "NAME=%%~nf"
    echo    - Building !NAME!.exe ...
    
    :: SHADER_PATH is "../examples/NAME.cpp" (relative to build dir)
    cl /nologo /openmp /O2 /EHsc /MD /D_USE_MATH_DEFINES /DSDL_MAIN_HANDLED /DLINK_SHADER /DUSE_OPENGL /DSHADER_PATH=\"../examples/!NAME!.cpp\" %CL_INCLUDES% /Febuild\!NAME!.exe main.cpp %%f /link %LIBS% shell32.lib user32.lib gdi32.lib
    if !errorlevel! neq 0 exit /b !errorlevel!
)

:: --- DEPLOY ---
echo.
echo [3/3] Deploying DLLs...
copy /Y "%VCPKG_ROOT%\arm64-windows\bin\*.dll" build\ >nul
if exist texture.jpg copy /Y texture.jpg build\ >nul

del build\*.obj

echo.
echo [SUCCESS] All builds complete! 
pause:
