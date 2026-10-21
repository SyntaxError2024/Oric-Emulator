@echo off
rem Fichier encode en windows-1252 pour Windows
echo ===================================================
echo Compilation de l'emulateur Oric Atmos avec MinGW64
echo ===================================================
echo.

set app="oric_atmos.exe"

taskkill /F /im %app% 2>nul

del %app% 2>nul

rem -O3       : active des optimisations poussées
rem -s        : supprime les symboles de débogage du fichier final
rem -static   : lie statiquement les bibliothèques lorsque possible
rem -municode : sélectionne le point d'entrée Unicode, ici wWinMain
rem -mwindows : crée une application graphique sans fenêtre console
rem -lgdi32   : lie la bibliothèque graphique Windows GDI
rem -lwinmm   : lie la bibliothèque multimédia Windows
rem -lshell32 : lie la bibliothèque Windows Shell

gcc -O3 -s -static -municode -mwindows common.c cpu.c memory.c via.c ay38912.c keyboard.c tape.c video.c main.c -o %app% -lgdi32 -lwinmm -lshell32

if %errorlevel% equ 0 (
    echo Compilation terminee avec succes !
) else (
    echo Erreur lors de la compilation !
)
