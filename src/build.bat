@echo off
rem Fichier encode en windows-1252 pour Windows
echo ===================================================
echo Compilation de l'emulateur Oric Atmos avec MinGW64
echo ===================================================
echo.

gcc -O3 oric_win32.c -o oric_atmos.exe -municode -lgdi32 -lwinmm -lshell32

echo Compilation terminee avec succes !
