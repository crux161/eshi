@echo off
setlocal enabledelayedexpansion

:: --- CONFIGURATION ---
set VCPKG_ROOT=C:\Users\lepus\git\eshi\vcpkg_installed
set CUDA_PATH=D:\CUDA

:: Compiler Flags
set NVCC_INCLUDES=-I. -I"%VCPKG_ROOT%\arm64-windows\include"
set CL_INCLUDES=/I. /I"%VCPKG_ROOT%\arm64-windows\include"

:: Linker Libs
set LIBS=/LIBPATH:"%VCPKG_ROOT%\arm64-windows\lib" /LIBPATH:"%CUDA_PATH%\lib\x64" SDL2.lib avcodec.lib avformat.lib avutil.lib swscale.lib cudart.lib

:: Clean build directory
if exist build rmdir /s /q build
mkdir build

echo [1/5] Compiling Shared Host Object (Main)...
:: This is reused by ALL executables
cl /c /nologo /openmp /O2 /EHsc /std:c++14 /MD /D_USE_MATH_DEFINES /DSDL_MAIN_HANDLED /DUSE_CUDA /DLINK_SHADER %CL_INCLUDES% /Fobuild\main.obj main.cpp
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- BUILD 1: The Main App (shader.cpp) ---
echo.
echo [2/5] Building Default: eshi.exe ...

:: 1. Compile CPU Shader
cl /c /nologo /openmp /O2 /EHsc /MD /D_USE_MATH_DEFINES %CL_INCLUDES% /Fobuild\shader.obj shader.cpp
if %errorlevel% neq 0 exit /b %errorlevel%

:: 2. Compile GPU Kernel (injecting shader.cpp path)
nvcc -O3 -c renderer_gpu.cu -o build\renderer_gpu.obj -ccbin cl -D_USE_MATH_DEFINES -DSHADER_PATH=\"shader.cpp\" -Xcompiler "/MD" %NVCC_INCLUDES%
if %errorlevel% neq 0 exit /b %errorlevel%

:: 3. Link
link /nologo /SUBSYSTEM:CONSOLE /OUT:build\eshi.exe build\main.obj build\shader.obj build\renderer_gpu.obj %LIBS% shell32.lib user32.lib gdi32.lib
if %errorlevel% neq 0 exit /b %errorlevel%

:: --- BUILD 2: The Examples (examples/*.cpp) ---
echo.
echo [3/5] Building Examples...

for %%f in (examples\*.cpp) do (
    set "NAME=%%~nf"
    echo    - Building !NAME!.exe ...
    
    :: 1. Compile CPU Part
    :: [FIX] Removed 'examples\' prefix because %%f already contains the path
    cl /c /nologo /O2 /EHsc /MD /D_USE_MATH_DEFINES %CL_INCLUDES% /Fobuild\!NAME!.obj %%f
    if !errorlevel! neq 0 exit /b !errorlevel!
    
    :: 2. Compile GPU Part (Injecting the example path)
    :: We explicitly use examples/!NAME!.cpp here because nvcc needs a string for the macro
    nvcc -O3 -c renderer_gpu.cu -o build\!NAME!_gpu.obj -ccbin cl -D_USE_MATH_DEFINES -DSHADER_PATH=\"examples/!NAME!.cpp\" -Xcompiler "/MD" %NVCC_INCLUDES%
    if !errorlevel! neq 0 exit /b !errorlevel!
    
    :: 3. Link
    link /nologo /SUBSYSTEM:CONSOLE /OUT:build\!NAME!.exe build\main.obj build\!NAME!.obj build\!NAME!_gpu.obj %LIBS% shell32.lib user32.lib gdi32.lib
    if !errorlevel! neq 0 exit /b !errorlevel!
)

:: --- DEPLOY ---
echo.
echo [4/5] Deploying DLLs...
copy /Y "%VCPKG_ROOT%\arm64-windows\bin\*.dll" build\ >nul
copy /Y "%CUDA_PATH%\bin\cudart64*.dll" build\ >nul

echo.
echo [5/5] Copying Resources...
if exist texture.jpg copy /Y texture.jpg build\ >nul

del build\*.obj

echo.
echo [SUCCESS] All builds complete! 
pause
