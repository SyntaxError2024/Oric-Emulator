#!/usr/bin/env bash

# abort on error

set -e

echo "compilation en cours"

app="oric_atmos.exe"

rm -f "$app"

x86_64-w64-mingw32-gcc -O3 -s -static -mwindows -municode\
  common.c cpu.c memory.c via.c ay38912.c keyboard.c \
  tape.c video.c main.c \
  -o "$app" -lgdi32 -lwinmm -lshell32
  
echo "compilation terminée"
