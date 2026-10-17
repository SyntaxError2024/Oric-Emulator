@echo off
rem Fichier encode en windows-1252 pour Windows
echo ===================================================
echo Sauvegarde et Push sur GitHub
echo ===================================================
echo.
git add .
git commit -m "Mise a jour automatique de l'emulateur Oric Atmos"
git push origin main
echo.
echo Operation terminee !
pause
