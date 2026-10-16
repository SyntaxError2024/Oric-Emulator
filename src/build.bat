@echo off
rem Fichier encode en windows-1252 pour Windows
echo ===================================================
echo Compilation de l'emulateur Oric Atmos avec MinGW64
echo ===================================================
echo.

set app="oric_atmos.exe"

del %app%

gcc -O3 oric_win32.c -o %app% -municode -lgdi32 -lwinmm -lshell32

echo Compilation terminee avec succes !
