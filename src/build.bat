@echo off
rem Fichier encode en windows-1252 pour Windows
echo ===================================================
echo Compilation de l'emulateur Oric Atmos avec MinGW64
echo ===================================================
echo.

set app="oric_atmos.exe"

taskkill /F /im %app% 2>nul

del %app% 2>nul

gcc -O3 -municode -mwindows common.c cpu.c memory.c via.c ay38912.c keyboard.c tape.c video.c main.c -o %app% -lgdi32 -lwinmm -lshell32

if %errorlevel% equ 0 (
    echo Compilation terminee avec succes !
) else (
    echo Erreur lors de la compilation !
)
