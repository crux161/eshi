@echo off
echo Cleaning build artifacts...

:: Delete specific file types recursively
del /s /q *.exe *.obj *.lib *.exp *.pdb *.ilk *.cudafe* *.ptx *.fatbin *.o 2>nul

:: Remove the build directory if it exists
if exist build rmdir /s /q build

echo Done. Only source code should remain.
pause
