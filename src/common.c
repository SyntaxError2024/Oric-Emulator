#include "common.h"

const uint32_t PALETTE[8] = {
    0x000000, /* 0: Noir */
    0xFF0000, /* 1: Rouge */
    0x00FF00, /* 2: Vert */
    0xFFFF00, /* 3: Jaune */
    0x0000FF, /* 4: Bleu */
    0xFF00FF, /* 5: Magenta */
    0x00FFFF, /* 6: Cyan */
    0xFFFFFF  /* 7: Blanc */
};

void debug_init(void) {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }
    FILE *fpOut = NULL, *fpErr = NULL;
    _wfreopen_s(&fpOut, L"CONOUT$", L"w", stdout);
    _wfreopen_s(&fpErr, L"CONOUT$", L"w", stderr);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleW(L"Oric Atmos Win32 - Console de Débogage");

    wprintf(L"===============================================================\n");
    wprintf(L"  Émulateur Oric Atmos Win32 (C Unicode / MinGW64)\n");
    wprintf(L"  Architecture modulaire (Son, Clavier, K7, Vidéo, CPU, VIA)\n");
    wprintf(L"  Console de débogage active (UTF-8 / Unicode)\n");
    wprintf(L"===============================================================\n\n");
}

void debug_log(const wchar_t *format, ...) {
    va_list args;
    va_start(args, format);
    wprintf(L"[DEBUG] ");
    vwprintf(format, args);
    wprintf(L"\n");
    va_end(args);
    fflush(stdout);
}

void ansi_to_wide(const char *src, wchar_t *dst, int max_len) {
    if (!src || !dst || max_len <= 0) return;
    int len = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, max_len);
    if (len <= 0) {
        int i = 0;
        while (src[i] && i < max_len - 1) {
            dst[i] = (wchar_t)(unsigned char)src[i];
            i++;
        }
        dst[i] = 0;
    }
}
