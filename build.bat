@echo off
chcp 1252 >nul
echo Compilation de l'emulateur Oric Atmos Win32 (MinGW64 C Unicode)...

gcc -O2 -municode -mwindows -s -static src/main.c src/cpu.c src/memory.c src/via.c src/ay38912.c src/keyboard.c src/tape.c src/video.c src/browser.c src/fdc.c src/common.c -o oric_atmos.exe -lgdi32 -lwinmm -lshell32

if %ERRORLEVEL% EQU 0 (
    echo Compilation reussie : oric_atmos.exe genere avec succes.
) else (
    echo Erreur lors de la compilation.
)
pause
