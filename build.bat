@echo off

set "app=oric_atmos.exe"

taskkill /f /im %app%

mingw32-make.exe

%app%