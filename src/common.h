#ifndef COMMON_H
#define COMMON_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#define SCREEN_W 240
#define SCREEN_H 224
#define SCALE_X  3
#define SCALE_Y  2
#define WIN_W    (SCREEN_W * SCALE_X)
#define WIN_H    (SCREEN_H * SCALE_Y)

#define CPU_CLOCK_HZ      1000000
#define FRAME_RATE_HZ     50
#define CYCLES_PER_FRAME  (CPU_CLOCK_HZ / FRAME_RATE_HZ)

/* Palette RVB standard Oric (8 couleurs) */
extern const uint32_t PALETTE[8];

/* Fonctions de débogage console Unicode */
void debug_init(void);
void debug_log(const wchar_t *format, ...);
void ansi_to_wide(const char *src, wchar_t *dst, int max_len);

#endif /* COMMON_H */
