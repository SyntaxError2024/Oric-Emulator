@echo off
rem Fichier encode en windows-1252 pour Windows
echo ===================================================
echo Compilation de l'emulateur Oric Atmos avec MinGW64
echo ===================================================
echo.

set app="oric_atmos.exe"

taskkill /F /im %app% 2>nul
del %app% 2>nul

rem -O3       : active des optimisations poussees
rem -s        : supprime les symboles de debogage du fichier final
rem -static   : lie statiquement les bibliotheques lorsque possible
rem -municode : selectionne le point d'entree Unicode, ici wWinMain
rem -mwindows : cree une application graphique sans fenetre console
rem -lgdi32   : lie la bibliotheque graphique Windows GDI
rem -lwinmm   : lie la bibliotheque multimedia Windows
rem -lshell32 : lie la bibliotheque Windows Shell

gcc -O3 -s -static -municode -mwindows common.c cpu.c memory.c via.c ay38912.c keyboard.c tape.c video.c main.c -o %app% -lgdi32 -lwinmm -lshell32

if %errorlevel% equ 0 (
    echo Compilation terminee avec succes !
) else (
    echo Erreur lors de la compilation !
)
