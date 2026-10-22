/*
 * ============================================================================
 * ORIC ATMOS - ÉMULATEUR WIN32 PUR (C UNICODE / MinGW64 / GDI)
 * ============================================================================
 * - Aucun framework tiers, aucune bibliothèque externe à Windows.
 * - Architecture modulaire :
 *     - common.h / common.c : Définitions globales, palette, logs console Unicode
 *     - cpu.h / cpu.c       : Émulateur processeur 6502 et hooks K7 ROM
 *     - memory.h / memory.c : Routage mémoire (RAM 64 Ko, ROM 16 Ko, Microdisc 8 Ko)
 *     - via.h / via.c       : Contrôleur VIA 6522 et compteurs Timer 1 / Timer 2
 *     - ay38912.h / ay38912.c : Synthèse sonore AY-3-8912 WaveOut et port I/O
 *     - keyboard.h / keyboard.c : Matrice clavier 8x8, AZERTY/QWERTY et auto-typein
 *     - tape.h / tape.c     : Gestionnaire de cassettes .TAP
 *     - video.h / video.c   : Rendu ULA TEXT et HIRES avec police et accents français
 *     - main.c              : Boucle de message Win32, glisser-déposer et plein écran
 * ============================================================================
 */

#include "common.h"
#include "cpu.h"
#include "memory.h"
#include "via.h"
#include "ay38912.h"
#include "keyboard.h"
#include "tape.h"
#include "video.h"

/* ============================================================================
 * PLEIN ÉCRAN (ALT+ENTRÉE) AVEC RESPECT DU RATIO 4:3
 * ============================================================================ */
static void toggle_fullscreen(HWND hwnd) {
    static WINDOWPLACEMENT wp = { sizeof(wp) };
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

    if (dwStyle & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &wp) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);

            int screen_w = mi.rcMonitor.right - mi.rcMonitor.left;
            int screen_h = mi.rcMonitor.bottom - mi.rcMonitor.top;

            int window_w, window_h;
            if (screen_w * 3 > screen_h * 4) {
                /* Écran plus large que 4:3 : caler sur la hauteur */
                window_h = screen_h;
                window_w = screen_h * 4 / 3;
            } else {
                /* Écran plus haut : caler sur la largeur */
                window_w = screen_w;
                window_h = screen_w * 3 / 4;
            }

            int margin_left = (screen_w - window_w) / 2;
            int margin_top  = (screen_h - window_h) / 2;

            SetWindowPos(hwnd, HWND_TOP,
                         margin_left, margin_top,
                         window_w, window_h,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            debug_log(L"Basculement en Mode Plein Écran (Ratio 4:3 respecté)");
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &wp);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        debug_log(L"Basculement en Mode Fenêtré");
    }
}

/* ============================================================================
 * PROCÉDURE DE FENÊTRE WIN32
 * ============================================================================ */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            video_init();
            audio_init(hwnd);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            wchar_t filename[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, filename, MAX_PATH)) {
                wchar_t *ext = wcsrchr(filename, L'.');
                if (ext && (_wcsicmp(ext, L".tap") == 0)) {
                    debug_log(L"Fichier .TAP déposé : %ls", filename);
                    if (tape_load_file(filename)) {
                        wchar_t title[512];
                        swprintf(title, 512, L"Oric Atmos - [K7 chargée : %ls]",
                                 g_tape.current_name[0] ? (wchar_t *)g_tape.current_name : L"Inconnu");
                        SetWindowTextW(hwnd, title);
                    }
                } else if (ext && (_wcsicmp(ext, L".dsk") == 0)) {
                    debug_log(L"Image disque .DSK chargée : %ls", filename);
                    g_microdis_control = 0x00;
                    SetWindowTextW(hwnd, L"Oric Atmos - [Microdisc Disk Loaded]");
                } else if (ext && (_wcsicmp(ext, L".rom") == 0)) {
                    debug_log(L"Fichier .ROM déposé : %ls", filename);
                    oric_load_rom(filename);
                    cpu_reset();
                    via_reset();
                    ay_reset();
                    keyboard_reset();
                    SetWindowTextW(hwnd, L"Oric Atmos - [Nouvelle ROM chargée]");
                }
            }
            DragFinish(hDrop);
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
            return 0;
        }

        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN) {
                toggle_fullscreen(hwnd);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000)) {
                toggle_fullscreen(hwnd);
                return 0;
            }
            keyboard_handle_vk((UINT)wParam, true);
            if (wParam == VK_F5) {
                debug_log(L"Réinitialisation matérielle (Reset F5)");
                cpu_reset();
                via_reset();
                ay_reset();
                keyboard_reset();
                tape_reset();
            } else if (wParam == VK_F1) {
                MessageBoxW(hwnd,
                    L"Émulateur Oric Atmos Win32 pur (MinGW64 / C Unicode)\n\n"
                    L"Commandes :\n"
                    L"- Glisser-déposer un fichier .TAP, .ROM ou .DSK\n"
                    L"- Alt+Entrée : Plein écran / Mode fenêtré (4:3)\n"
                    L"- F5 : Réinitialisation de la machine\n"
                    L"- Clavier AZERTY / QWERTY complet\n",
                    L"Aide Oric Atmos", MB_OK | MB_ICONINFORMATION);
            }
            return 0;

        case WM_KEYUP:
            keyboard_handle_vk((UINT)wParam, false);
            return 0;

        case WM_CHAR: {
            wchar_t wch = (wchar_t)wParam;
            if (wch >= 32 && wch <= 126) {
                keyboard_queue_char((char)wch);
            } else if (wch == L'\r' || wch == L'\n') {
                keyboard_queue_char('\r');
            } else if (wch == 8) {
                keyboard_queue_char('\x08');
            } else if (wch == 27) {
                keyboard_queue_char('\x1b');
            }
            return 0;
        }

        case WM_SETFOCUS:
            return 0;

        case WM_KILLFOCUS:
            keyboard_reset();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            StretchDIBits(
                hdc,
                0, 0, rc.right, rc.bottom,
                0, 0, SCREEN_W, SCREEN_H,
                g_framebuffer,
                &g_bmi,
                DIB_RGB_COLORS,
                SRCCOPY
            );
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            audio_shutdown();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ============================================================================
 * EXÉCUTION DE L'ÉMULATEUR
 * ============================================================================ */
static int run_emulator(HINSTANCE hInstance, const wchar_t *cmdLine, int nCmdShow) {
    debug_init();

    tape_init();
    keyboard_reset();

    bool rom_explicitly_loaded = false;
    if (cmdLine && cmdLine[0] != 0) {
        wchar_t path[MAX_PATH];
        wcsncpy(path, cmdLine, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
        if (path[0] == L'\"') {
            size_t l = wcslen(path);
            if (path[l - 1] == L'\"') path[l - 1] = 0;
            wmemmove(path, path + 1, l);
        }

        wchar_t *ext = wcsrchr(path, L'.');
        if (ext && (_wcsicmp(ext, L".rom") == 0)) {
            oric_load_rom(path);
            if (g_rom_loaded) rom_explicitly_loaded = true;
        }
    }

    if (!rom_explicitly_loaded) {
        oric_load_rom(L"./roms/atmos.rom");
    }

    /* Charger automatiquement MICRODIS.ROM s'il est présent */
    oric_load_microdis_rom(L"./roms/MICRODIS.ROM");

    if (!g_rom_loaded) {
        debug_log(L"Erreur : Aucune ROM trouvée dans le répertoire courant !");
        MessageBoxW(NULL,
            L"Impossible de trouver 'atmos.rom'.",
            L"ROM non trouvée", MB_OK | MB_ICONWARNING);
    }

    memory_reset();
    via_reset();
    ay_reset();
    cpu_reset();

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OricAtmosWin32";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    if (!RegisterClassW(&wc)) {
        debug_log(L"Erreur : Échec de l'enregistrement de la classe WNDCLASSW");
        return 1;
    }

    RECT rc = {0, 0, WIN_W, WIN_H};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"OricAtmosWin32",
        L"Oric Atmos (MinGW64 / C Unicode / GDI) - Alt+Entrée Plein écran",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        debug_log(L"Erreur : Échec de création de la fenêtre Win32");
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    debug_log(L"Fenêtre d'émulation ouverte (%dx%d)", rc.right - rc.left, rc.bottom - rc.top);

    /* Charger un fichier TAP ou DSK passé en argument s'il existe */
    if (cmdLine && cmdLine[0] != 0) {
        wchar_t path[MAX_PATH];
        wcsncpy(path, cmdLine, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
        if (path[0] == L'\"') {
            size_t l = wcslen(path);
            if (path[l - 1] == L'\"') path[l - 1] = 0;
            wmemmove(path, path + 1, l);
        }

        wchar_t *ext = wcsrchr(path, L'.');
        if (ext) {
            if (_wcsicmp(ext, L".tap") == 0) {
                tape_load_file(path);
                wchar_t title[512];
                swprintf(title, 512, L"Oric Atmos - [K7 chargée : %ls]",
                         g_tape.current_name[0] ? (wchar_t *)g_tape.current_name : L"Inconnu");
                SetWindowTextW(hwnd, title);
            } else if (_wcsicmp(ext, L".dsk") == 0) {
                debug_log(L"Image disque .DSK chargée : %ls", path);
                g_microdis_control = 0x00;
                SetWindowTextW(hwnd, L"Oric Atmos - [Microdisc Disk Loaded]");
            }
        }
    }

    timeBeginPeriod(1);
    DWORD last_tick = timeGetTime();

    MSG msg;
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!running) break;

        /* Émulation de la trame 50Hz (accélération turbo x15 pendant lecture cassette) */
        bool tape_running = (g_via.orb & 0x40) && g_tape.loaded && (g_tape.pulse_pos < g_tape.pulse_count);
        uint32_t frame_cycles = tape_running ? (CYCLES_PER_FRAME * 15) : CYCLES_PER_FRAME;
        uint64_t target_cycles = g_cpu.cycles + frame_cycles;
        while (g_cpu.cycles < target_cycles) {
            uint64_t before = g_cpu.cycles;
            cpu_step();
            uint32_t elapsed = (uint32_t)(g_cpu.cycles - before);
            via_step(elapsed);
        }

        video_render_frame();
        if (!tape_running) {
            audio_update();
        }
        InvalidateRect(hwnd, NULL, FALSE);

        DWORD now = timeGetTime();
        DWORD elapsed_ms = now - last_tick;
        if (elapsed_ms < 20) {
            Sleep(20 - elapsed_ms);
        }
        last_tick = timeGetTime();
    }

    timeEndPeriod(1);
    debug_log(L"Fermeture de l'émulateur Oric Atmos");
    return (int)msg.wParam;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;

    wchar_t path[MAX_PATH];

    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);

    if (n == 0 || n >= MAX_PATH)
        return 1;
    
    

    wchar_t *slash = wcsrchr(path, L'\\');
    
    if (slash == NULL)
        return 1;

    // *slash = L'\0';

    *slash=0;

    if (!SetCurrentDirectoryW(path))
        return 1;

    return run_emulator(hInstance, lpCmdLine, nCmdShow);
}

/*
int wmain(int argc, wchar_t *argv[]) {
    HINSTANCE hInst = GetModuleHandleW(NULL);
    const wchar_t *real_arg = (argc > 1) ? argv[1] : L"";
    return run_emulator(hInst, real_arg, SW_SHOWNORMAL);
}
*/