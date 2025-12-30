@echo off
setlocal enabledelayedexpansion

:: --- CONFIGURATION ---
:: Adjusted to the path you provided for the X13s
set VCPKG_ROOT=C:\Users\lepus\git\eshi\vcpkg_installed

:: Compiler Flags
:: removed /DUSE_CUDA and CUDA includes
set CL_INCLUDES=/I. /I"%VCPKG_ROOT%\arm64-windows\include"

:: Linker Libs
:: Removed cudart.lib and CUDA lib paths
set LIBS=/LIBPATH:"%VCPKG_ROOT%\arm64-windows\lib" SDL2.lib avcodec.lib avformat.lib avutil.lib swscale.lib

:: Clean build directory
if exist build rmdir /s /q build
mkdir build

echo [1/4] Compiling Shared Host Object (Main)...
:: Removed /DUSE_CUDA flag so main.cpp ignores the GPU code blocks
cl /c /nologo /openmp /O2 /EHsc /std:c++14 /MD /D_USE_MATH_DEFINES /DSDL_MAIN_HANDLED /DLINK_SHADER %CL_INCLUDES% /Fobuild\main.obj main.cpp
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- BUILD 1: The Main App (shader.cpp) ---
echo.
echo [2/4] Building Default: eshi.exe ...

:: 1. Compile CPU Shader
cl /c /nologo /openmp /O2 /EHsc /MD /D_USE_MATH_DEFINES %CL_INCLUDES% /Fobuild\shader.obj shader.cpp
if %errorlevel% neq 0 exit /b %errorlevel%

:: 2. Link (Removed renderer_gpu.obj)
link /nologo /SUBSYSTEM:CONSOLE /OUT:build\eshi.exe build\main.obj build\shader.obj %LIBS% shell32.lib user32.lib gdi32.lib
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- BUILD 2: The Examples (examples/*.cpp) ---
echo.
echo [3/4] Building Examples...

for %%f in (examples\*.cpp) do (
    set "NAME=%%~nf"
    echo    - Building !NAME!.exe ...
    
    :: 1. Compile CPU Part
    cl /c /nologo /O2 /EHsc /MD /D_USE_MATH_DEFINES %CL_INCLUDES% /Fobuild\!NAME!.obj %%f
    if !errorlevel! neq 0 exit /b !errorlevel!
    
    :: 2. Link (Removed GPU obj dependencies)
    link /nologo /SUBSYSTEM:CONSOLE /OUT:build\!NAME!.exe build\main.obj build\!NAME!.obj %LIBS% shell32.lib user32.lib gdi32.lib
    if !errorlevel! neq 0 exit /b !errorlevel!
)

:: --- DEPLOY ---
echo.
echo [4/4] Deploying DLLs...
copy /Y "%VCPKG_ROOT%\arm64-windows\bin\*.dll" build\ >nul
:: Removed CUDA DLL copy steps

echo.
echo [5/5] Copying Resources...
if exist texture.jpg copy /Y texture.jpg build\ >nul

del build\*.obj

echo.
echo [SUCCESS] All builds complete! 
pause