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
#include "browser.h"

/* ============================================================================
 * PLEIN ÉCRAN ET MISE À L'ÉCHELLE ENTIÈRE (PIXEL-PERFECT) CENTRÉE (RATIO 4:3)
 * ============================================================================ */
static bool g_is_fullscreen = false;
static WINDOWPLACEMENT g_saved_wp = { sizeof(g_saved_wp) };

static void get_display_rect(HWND hwnd, RECT *out_disp_rc, RECT *out_client_rc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    if (out_client_rc) *out_client_rc = rc;

    int client_w = rc.right - rc.left;
    int client_h = rc.bottom - rc.top;

    if (client_w <= 0 || client_h <= 0) {
        SetRect(out_disp_rc, 0, 0, 0, 0);
        return;
    }

    /* Facteur d'échelle entier : chaque pixel Oric correspond à 1, 2, 3, 4... pixels réels du moniteur */
    int scale_x = client_w / SCREEN_W;
    int scale_y = client_h / SCREEN_H;
    int scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1) scale = 1;

    int draw_w = SCREEN_W * scale;
    int draw_h = SCREEN_H * scale;

    int dst_x = (client_w - draw_w) / 2;
    int dst_y = (client_h - draw_h) / 2;

    out_disp_rc->left = dst_x;
    out_disp_rc->top = dst_y;
    out_disp_rc->right = dst_x + draw_w;
    out_disp_rc->bottom = dst_y + draw_h;
}

static void toggle_fullscreen(HWND hwnd) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

    if (!g_is_fullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (GetWindowPlacement(hwnd, &g_saved_wp) && GetMonitorInfo(hMon, &mi)) {
            g_is_fullscreen = true;

            int mon_x = mi.rcMonitor.left;
            int mon_y = mi.rcMonitor.top;
            int mon_w = mi.rcMonitor.right - mi.rcMonitor.left;
            int mon_h = mi.rcMonitor.bottom - mi.rcMonitor.top;

            /* La fenêtre plein écran couvre l'intégralité du moniteur (aucun bureau visible) */
            SetWindowLong(hwnd, GWL_STYLE, (dwStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
            SetWindowPos(hwnd, HWND_TOP,
                         mon_x, mon_y, mon_w, mon_h,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

            int scale = mon_h / SCREEN_H;
            if (mon_w / SCREEN_W < scale) scale = mon_w / SCREEN_W;
            if (scale < 1) scale = 1;

            debug_log(L"Plein écran activé : %dx%d (Échelle Oric entière: %dx, pixels réels: %dx%d)",
                      mon_w, mon_h, scale, SCREEN_W * scale, SCREEN_H * scale);
        }
    } else {
        g_is_fullscreen = false;

        /* Rétablissement du mode fenêtré standard avec bordures et barre de titre */
        SetWindowLong(hwnd, GWL_STYLE, (dwStyle & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_saved_wp);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        debug_log(L"Mode fenêtré rétabli");
    }

    InvalidateRect(hwnd, NULL, TRUE);
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
            if ((wParam == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000)) || wParam == VK_F11) {
                toggle_fullscreen(hwnd);
                return 0;
            }
            if (wParam == VK_F2) {
                g_browser.active = !g_browser.active;
                if (g_browser.active) {
                    browser_refresh();
                }
                return 0;
            }
            if (g_browser.active) {
                if (browser_handle_key((UINT)wParam, hwnd)) {
                    return 0;
                }
            }
            if (wParam == VK_ESCAPE && g_is_fullscreen) {
                toggle_fullscreen(hwnd);
                return 0;
            }
            if (wParam == VK_F5) {
                debug_log(L"Réinitialisation matérielle (Reset F5)");
                cpu_reset();
                via_reset();
                ay_reset();
                keyboard_reset();
                tape_reset();
                return 0;
            }
            if (wParam == VK_F1) {
                MessageBoxW(hwnd,
                    L"Émulateur Oric Atmos Win32 pur (MinGW64 / C Unicode)\n\n"
                    L"Commandes :\n"
                    L"- F2 : Ouvrir / Fermer l'explorateur de fichiers Oric\n"
                    L"- Glisser-déposer un fichier .TAP, .ROM ou .DSK\n"
                    L"- Alt+Entrée ou F11 : Plein écran (centré, ratio 4:3, pixels entiers)\n"
                    L"- Échap : Quitter le plein écran\n"
                    L"- F5 : Réinitialisation matérielle (Reset)\n"
                    L"- Clavier AZERTY / QWERTY complet\n",
                    L"Aide Oric Atmos", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            if (lParam & 0x40000000) {
                /* Touche déjà enfoncée (autorépétition OS Windows) :
                 * on l'ignore car la ROM Oric Atmos gère elle-même la répétition matérielle à 100 Hz */
                return 0;
            }
            keyboard_handle_vk((UINT)wParam, true);
            return 0;

        case WM_KEYUP:
            keyboard_handle_vk((UINT)wParam, false);
            return 0;

        case WM_CHAR:
            /* La saisie du clavier physique est gérée directement sur la matrice par WM_KEYDOWN / WM_KEYUP.
             * On ne met pas en file d'attente dans WM_CHAR afin d'éviter tout dédoublement de caractère. */
            return 0;

        case WM_SETFOCUS:
            return 0;

        case WM_KILLFOCUS:
            keyboard_reset();
            return 0;

        case WM_ERASEBKGND:
            return 1; /* Évite tout scintillement, WM_PAINT gère toute la surface */

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT disp_rc, client_rc;
            get_display_rect(hwnd, &disp_rc, &client_rc);

            int client_w = client_rc.right - client_rc.left;
            int client_h = client_rc.bottom - client_rc.top;
            int draw_w = disp_rc.right - disp_rc.left;
            int draw_h = disp_rc.bottom - disp_rc.top;

            HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);

            /* Bandes noires (letterbox / pillarbox) pour centrer le ratio 4:3 de l'Oric */
            if (disp_rc.left > 0) {
                RECT r_left  = { 0, 0, disp_rc.left, client_h };
                RECT r_right = { disp_rc.right, 0, client_w, client_h };
                FillRect(hdc, &r_left, black_brush);
                FillRect(hdc, &r_right, black_brush);
            }
            if (disp_rc.top > 0) {
                RECT r_top    = { 0, 0, client_w, disp_rc.top };
                RECT r_bottom = { 0, disp_rc.bottom, client_w, client_h };
                FillRect(hdc, &r_top, black_brush);
                FillRect(hdc, &r_bottom, black_brush);
            }

            /* Affichage pixel-perfect : chaque pixel Oric = 1, 2, 3 ou N pixels réels sans flou */
            SetStretchBltMode(hdc, COLORONCOLOR);
            StretchDIBits(
                hdc,
                disp_rc.left, disp_rc.top, draw_w, draw_h,
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
    browser_init();

    bool rom_explicitly_loaded = false;
    bool file_passed_on_cli = false;

    if (cmdLine && cmdLine[0] != 0) {
        wchar_t path[MAX_PATH];
        wcsncpy(path, cmdLine, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
        wchar_t *p = path;
        while (*p == L' ' || *p == L'\t') p++;
        size_t l = wcslen(p);
        while (l > 0 && (p[l - 1] == L' ' || p[l - 1] == L'\t')) {
            p[--l] = 0;
        }
        if (l >= 2 && p[0] == L'\"' && p[l - 1] == L'\"') {
            p[l - 1] = 0;
            p++;
        }

        wchar_t *ext = wcsrchr(p, L'.');
        if (ext && (_wcsicmp(ext, L".rom") == 0)) {
            if (oric_load_rom(p)) {
                rom_explicitly_loaded = true;
                file_passed_on_cli = true;
            }
        }
    }

    /* Chargement de la ROM par défaut mémorisée dans settings.ini, sinon atmos.rom */
    if (!rom_explicitly_loaded) {
        if (g_browser.default_rom[0] != 0) {
            oric_load_rom(g_browser.default_rom);
        }
        if (!g_rom_loaded) {
            if (!oric_load_rom(L"./roms/atmos.rom")) {
                if (!oric_load_rom(L"../roms/atmos.rom")) {
                    oric_load_rom(L"atmos.rom");
                }
            }
        }
    }

    /* Charger automatiquement MICRODIS.ROM s'il est présent */
    if (!oric_load_microdis_rom(L"./roms/MICRODIS.ROM")) {
        if (!oric_load_microdis_rom(L"../roms/MICRODIS.ROM")) {
            oric_load_microdis_rom(L"MICRODIS.ROM");
        }
    }

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
        L"Oric Atmos (MinGW64 / C Unicode / GDI) - F2 Explorateur - Alt+Entrée Plein écran",
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
        wchar_t *p = path;
        while (*p == L' ' || *p == L'\t') p++;
        size_t l = wcslen(p);
        while (l > 0 && (p[l - 1] == L' ' || p[l - 1] == L'\t')) {
            p[--l] = 0;
        }
        if (l >= 2 && p[0] == L'\"' && p[l - 1] == L'\"') {
            p[l - 1] = 0;
            p++;
        }

        wchar_t *ext = wcsrchr(p, L'.');
        if (ext) {
            if (_wcsicmp(ext, L".tap") == 0) {
                if (tape_load_file(p)) {
                    file_passed_on_cli = true;
                    wchar_t title[512];
                    swprintf(title, 512, L"Oric Atmos - [K7 chargée : %ls]",
                             g_tape.current_name[0] ? (wchar_t *)g_tape.current_name : L"Inconnu");
                    SetWindowTextW(hwnd, title);
                }
            } else if (_wcsicmp(ext, L".dsk") == 0) {
                debug_log(L"Image disque .DSK chargée : %ls", p);
                g_microdis_control = 0x00;
                file_passed_on_cli = true;
                SetWindowTextW(hwnd, L"Oric Atmos - [Microdisc Disk Loaded]");
            }
        }
    }

    /* Si un fichier a été passé explicitement sur la ligne de commande,
     * on désactive l'explorateur pour lancer directement le fichier.
     * Sinon, l'explorateur s'affiche au démarrage comme demandé ! */
    if (file_passed_on_cli) {
        g_browser.active = false;
    } else {
        g_browser.active = true;
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

        /* Si l'explorateur de fichiers est actif, on l'affiche et on met en pause l'émulation CPU */
        if (g_browser.active) {
            browser_render();
            video_render_frame();
            InvalidateRect(hwnd, NULL, FALSE);

            DWORD now = timeGetTime();
            DWORD elapsed_ms = now - last_tick;
            if (elapsed_ms < 20) {
                Sleep(20 - elapsed_ms);
            }
            last_tick = timeGetTime();
            continue;
        }

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