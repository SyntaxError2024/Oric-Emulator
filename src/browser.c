#include "browser.h"
#include "memory.h"
#include "cpu.h"
#include "via.h"
#include "ay38912.h"
#include "tape.h"

FileBrowser g_browser;

static void get_ini_path(wchar_t *out_path, size_t max_len) {
    GetModuleFileNameW(NULL, out_path, (DWORD)max_len);
    wchar_t *slash = wcsrchr(out_path, L'\\');
    if (slash) {
        *(slash + 1) = 0;
    }
    wcsncat(out_path, L"settings.ini", max_len - wcslen(out_path) - 1);
}

void browser_load_settings(void) {
    wchar_t ini_path[MAX_PATH];
    get_ini_path(ini_path, MAX_PATH);

    GetPrivateProfileStringW(L"Settings", L"LastDirectory", L"",
                             g_browser.current_dir, MAX_PATH, ini_path);
    GetPrivateProfileStringW(L"Settings", L"DefaultRom", L"",
                             g_browser.default_rom, MAX_PATH, ini_path);

    /* Si le dossier sauvegardé n'est pas valide ou est vide, prendre le répertoire courant */
    if (g_browser.current_dir[0] == 0 ||
        GetFileAttributesW(g_browser.current_dir) == INVALID_FILE_ATTRIBUTES) {
        GetCurrentDirectoryW(MAX_PATH, g_browser.current_dir);
    }
}

void browser_save_settings(void) {
    wchar_t ini_path[MAX_PATH];
    get_ini_path(ini_path, MAX_PATH);

    WritePrivateProfileStringW(L"Settings", L"LastDirectory",
                               g_browser.current_dir, ini_path);
    WritePrivateProfileStringW(L"Settings", L"DefaultRom",
                               g_browser.default_rom, ini_path);
}

static int compare_entries(const void *a, const void *b) {
    const BrowserEntry *ea = (const BrowserEntry *)a;
    const BrowserEntry *eb = (const BrowserEntry *)b;

    /* Le dossier parent [..] est toujours en tête */
    if (ea->type == ENTRY_UP) return -1;
    if (eb->type == ENTRY_UP) return 1;

    /* Les dossiers viennent avant les fichiers */
    bool a_is_dir = (ea->type == ENTRY_DIR);
    bool b_is_dir = (eb->type == ENTRY_DIR);
    if (a_is_dir && !b_is_dir) return -1;
    if (!a_is_dir && b_is_dir) return 1;

    return _wcsicmp(ea->name, eb->name);
}

void browser_refresh(void) {
    g_browser.count = 0;
    g_browser.selected = 0;
    g_browser.scroll_offset = 0;

    /* Ajouter le dossier parent [..] si nous ne sommes pas à la racine */
    size_t dlen = wcslen(g_browser.current_dir);
    bool is_root = false;
    if (dlen <= 3 && g_browser.current_dir[1] == L':') is_root = true;
    if (dlen == 1 && (g_browser.current_dir[0] == L'\\' || g_browser.current_dir[0] == L'/')) is_root = true;

    if (!is_root && g_browser.count < MAX_BROWSER_ENTRIES) {
        wcsncpy(g_browser.entries[g_browser.count].name, L"..", MAX_PATH - 1);
        g_browser.entries[g_browser.count].type = ENTRY_UP;
        g_browser.count++;
    }

    /* Parcours du répertoire avec FindFirstFileW */
    wchar_t pattern[MAX_PATH];
    wcsncpy(pattern, g_browser.current_dir, MAX_PATH - 1);
    pattern[MAX_PATH - 1] = 0;
    size_t pl = wcslen(pattern);
    if (pl > 0 && pattern[pl - 1] != L'\\' && pattern[pl - 1] != L'/') {
        wcsncat(pattern, L"\\*.*", MAX_PATH - pl - 1);
    } else {
        wcsncat(pattern, L"*.*", MAX_PATH - pl - 1);
    }

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
                continue;
            }

            if (g_browser.count >= MAX_BROWSER_ENTRIES) break;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wcsncpy(g_browser.entries[g_browser.count].name, fd.cFileName, MAX_PATH - 1);
                g_browser.entries[g_browser.count].type = ENTRY_DIR;
                g_browser.count++;
            } else {
                wchar_t *dot = wcsrchr(fd.cFileName, L'.');
                if (dot) {
                    BrowserEntryType type;
                    bool matched = false;
                    if (_wcsicmp(dot, L".tap") == 0) {
                        type = ENTRY_TAP;
                        matched = true;
                    } else if (_wcsicmp(dot, L".dsk") == 0) {
                        type = ENTRY_DSK;
                        matched = true;
                    } else if (_wcsicmp(dot, L".rom") == 0) {
                        type = ENTRY_ROM;
                        matched = true;
                    }

                    if (matched) {
                        wcsncpy(g_browser.entries[g_browser.count].name, fd.cFileName, MAX_PATH - 1);
                        g_browser.entries[g_browser.count].type = type;
                        g_browser.count++;
                    }
                }
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    /* Trier : .. d'abord, puis dossiers, puis fichiers */
    if (g_browser.count > 1) {
        qsort(g_browser.entries, g_browser.count, sizeof(BrowserEntry), compare_entries);
    }
}

void browser_init(void) {
    memset(&g_browser, 0, sizeof(g_browser));
    g_browser.active = true;
    browser_load_settings();
    browser_refresh();
}

#define BROWSER_VISIBLE_ROWS 22

/* Nettoyage des caractères pour l'affichage écran Oric (ASCII standard 32..126) */
static void sanitize_for_oric(char *str) {
    for (; *str; str++) {
        unsigned char c = (unsigned char)*str;
        if (c >= 128) {
            switch (c) {
                case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: *str = 'A'; break;
                case 0xC8: case 0xC9: case 0xCA: case 0xCB:                         *str = 'E'; break;
                case 0xCC: case 0xCD: case 0xCE: case 0xCF:                         *str = 'I'; break;
                case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6:             *str = 'O'; break;
                case 0xD9: case 0xDA: case 0xDB: case 0xDC:                         *str = 'U'; break;
                case 0xC7:                                                         *str = 'C'; break;
                case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: *str = 'a'; break;
                case 0xE8: case 0xE9: case 0xEA: case 0xEB:                         *str = 'e'; break;
                case 0xEC: case 0xED: case 0xEE: case 0xEF:                         *str = 'i'; break;
                case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6:             *str = 'o'; break;
                case 0xF9: case 0xFA: case 0xFB: case 0xFC:                         *str = 'u'; break;
                case 0xE7:                                                         *str = 'c'; break;
                default:                                                           *str = '?'; break;
            }
        } else if (c < 32) {
            *str = ' ';
        }
    }
}

void browser_render(void) {
    /* Écran Oric en mémoire vidéo : 28 lignes de 40 colonnes en 0xBB80 */
    uint8_t *screen = &g_ram[0xBB80];
    memset(screen, 0x20, 28 * 40);

    /* Ligne 0 : Bannière supérieure (Fond bleu 16+4, Texte blanc 7) */
    screen[0 * 40 + 0] = 16 + 4; /* Fond bleu */
    screen[0 * 40 + 1] = 7;      /* Texte blanc */
    const char *header = "  *** EXPLORATEUR DE FICHIERS ORIC ***  ";
    for (int i = 0; i < 38 && header[i]; i++) {
        screen[0 * 40 + 2 + i] = (uint8_t)header[i];
    }

    /* Ligne 1 : Chemin du dossier courant (Fond noir 16+0, Texte cyan 6) */
    screen[1 * 40 + 0] = 16 + 0;
    screen[1 * 40 + 1] = 6; /* Texte cyan */
    char dir_str[40];
    char conv_dir[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, g_browser.current_dir, -1, conv_dir, MAX_PATH, NULL, NULL);
    sanitize_for_oric(conv_dir);
    snprintf(dir_str, sizeof(dir_str), "Dossier: %.30s", conv_dir);
    for (int i = 0; i < 38 && dir_str[i]; i++) {
        screen[1 * 40 + 2 + i] = (uint8_t)dir_str[i];
    }

    /* Lignes 3 à 24 : Liste des fichiers et dossiers (22 lignes) */
    int start_y = 3;
    if (g_browser.count == 0) {
        screen[start_y * 40 + 0] = 16 + 0;
        screen[start_y * 40 + 1] = 1; /* Rouge */
        const char *empty_msg = "(Aucun fichier TAP/DSK/ROM trouve)";
        for (int i = 0; i < 38 && empty_msg[i]; i++) {
            screen[start_y * 40 + 2 + i] = (uint8_t)empty_msg[i];
        }
    } else {
        for (int r = 0; r < BROWSER_VISIBLE_ROWS; r++) {
            int idx = g_browser.scroll_offset + r;
            int line_y = start_y + r;
            if (idx >= g_browser.count) break;

            BrowserEntry *e = &g_browser.entries[idx];
            bool is_sel = (idx == g_browser.selected);

            /* Attributs de couleur */
            if (is_sel) {
                screen[line_y * 40 + 0] = 16 + 1; /* Fond rouge sélection */
                screen[line_y * 40 + 1] = 7;      /* Texte blanc */
            } else {
                screen[line_y * 40 + 0] = 16 + 0; /* Fond noir */
                switch (e->type) {
                    case ENTRY_UP:  screen[line_y * 40 + 1] = 2; break; /* Vert */
                    case ENTRY_DIR: screen[line_y * 40 + 1] = 2; break; /* Vert */
                    case ENTRY_TAP: screen[line_y * 40 + 1] = 3; break; /* Jaune */
                    case ENTRY_DSK: screen[line_y * 40 + 1] = 6; break; /* Cyan */
                    case ENTRY_ROM: screen[line_y * 40 + 1] = 5; break; /* Magenta */
                }
            }

            /* Curseur '>' */
            screen[line_y * 40 + 2] = is_sel ? '>' : ' ';

            /* Libellé */
            char name_ansi[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, e->name, -1, name_ansi, MAX_PATH, NULL, NULL);
            sanitize_for_oric(name_ansi);

            char line_text[40];
            if (e->type == ENTRY_UP) {
                snprintf(line_text, sizeof(line_text), "[..] (Dossier parent)");
            } else if (e->type == ENTRY_DIR) {
                snprintf(line_text, sizeof(line_text), "[DIR] %.28s", name_ansi);
            } else if (e->type == ENTRY_TAP) {
                snprintf(line_text, sizeof(line_text), "[TAP] %.28s", name_ansi);
            } else if (e->type == ENTRY_DSK) {
                snprintf(line_text, sizeof(line_text), "[DSK] %.28s", name_ansi);
            } else if (e->type == ENTRY_ROM) {
                snprintf(line_text, sizeof(line_text), "[ROM] %.28s", name_ansi);
            }

            for (int i = 0; i < 35 && line_text[i]; i++) {
                screen[line_y * 40 + 4 + i] = (uint8_t)line_text[i];
            }
        }
    }

    /* Ligne 26 : Aide touches (Fond noir, Texte blanc) */
    screen[26 * 40 + 0] = 16 + 0;
    screen[26 * 40 + 1] = 7;
    const char *help1 = " [Haut/Bas] Choisir   [Entree] Lancer  ";
    for (int i = 0; i < 38 && help1[i]; i++) {
        screen[26 * 40 + 2 + i] = (uint8_t)help1[i];
    }

    /* Ligne 27 : Aide quitter (Fond noir, Texte jaune) */
    screen[27 * 40 + 0] = 16 + 0;
    screen[27 * 40 + 1] = 3;
    const char *help2 = " [Echap] Quitter explorateur (BASIC)   ";
    for (int i = 0; i < 38 && help2[i]; i++) {
        screen[27 * 40 + 2 + i] = (uint8_t)help2[i];
    }
}

static void update_scroll(void) {
    if (g_browser.selected < g_browser.scroll_offset) {
        g_browser.scroll_offset = g_browser.selected;
    } else if (g_browser.selected >= g_browser.scroll_offset + BROWSER_VISIBLE_ROWS) {
        g_browser.scroll_offset = g_browser.selected - BROWSER_VISIBLE_ROWS + 1;
    }
}

bool browser_handle_key(UINT vk, HWND hwnd) {
    if (!g_browser.active) return false;

    switch (vk) {
        case VK_UP:
            if (g_browser.selected > 0) {
                g_browser.selected--;
                update_scroll();
            }
            return true;

        case VK_DOWN:
            if (g_browser.selected < g_browser.count - 1) {
                g_browser.selected++;
                update_scroll();
            }
            return true;

        case VK_PRIOR: /* Page Up */
            g_browser.selected -= BROWSER_VISIBLE_ROWS;
            if (g_browser.selected < 0) g_browser.selected = 0;
            update_scroll();
            return true;

        case VK_NEXT: /* Page Down */
            g_browser.selected += BROWSER_VISIBLE_ROWS;
            if (g_browser.selected >= g_browser.count) {
                g_browser.selected = (g_browser.count > 0) ? (g_browser.count - 1) : 0;
            }
            update_scroll();
            return true;

        case VK_ESCAPE:
            /* Fermer l'explorateur et lancer le boot BASIC normal */
            g_browser.active = false;
            return true;

        case VK_RETURN: {
            if (g_browser.count == 0) return true;
            BrowserEntry *e = &g_browser.entries[g_browser.selected];

            if (e->type == ENTRY_UP) {
                /* Remonter au dossier parent */
                wchar_t *slash = wcsrchr(g_browser.current_dir, L'\\');
                if (!slash) slash = wcsrchr(g_browser.current_dir, L'/');
                if (slash) {
                    if (slash == g_browser.current_dir + 2 && g_browser.current_dir[1] == L':') {
                        *(slash + 1) = 0; /* Garde 'C:\' */
                    } else if (slash == g_browser.current_dir) {
                        *(slash + 1) = 0; /* Garde '\' */
                    } else {
                        *slash = 0;
                    }
                }
                browser_save_settings();
                browser_refresh();
                return true;
            } else if (e->type == ENTRY_DIR) {
                /* Entrer dans le sous-dossier */
                size_t l = wcslen(g_browser.current_dir);
                if (l > 0 && g_browser.current_dir[l - 1] != L'\\' && g_browser.current_dir[l - 1] != L'/') {
                    wcsncat(g_browser.current_dir, L"\\", MAX_PATH - l - 1);
                }
                wcsncat(g_browser.current_dir, e->name, MAX_PATH - wcslen(g_browser.current_dir) - 1);
                browser_save_settings();
                browser_refresh();
                return true;
            }

            /* Fichier sélectionné (ROM, TAP ou DSK) : construire le chemin complet */
            wchar_t full_path[MAX_PATH];
            wcsncpy(full_path, g_browser.current_dir, MAX_PATH - 1);
            full_path[MAX_PATH - 1] = 0;
            size_t fl = wcslen(full_path);
            if (fl > 0 && full_path[fl - 1] != L'\\' && full_path[fl - 1] != L'/') {
                wcsncat(full_path, L"\\", MAX_PATH - fl - 1);
            }
            wcsncat(full_path, e->name, MAX_PATH - wcslen(full_path) - 1);

            /* Mémoriser le dossier dans settings.ini */
            browser_save_settings();

            if (e->type == ENTRY_ROM) {
                /* Mémoriser la ROM par défaut pour les prochains démarrages */
                wcsncpy(g_browser.default_rom, full_path, MAX_PATH - 1);
                g_browser.default_rom[MAX_PATH - 1] = 0;
                browser_save_settings();

                oric_load_rom(full_path);
                memory_reset();
                via_reset();
                ay_reset();
                cpu_reset();
                g_browser.active = false;
                SetWindowTextW(hwnd, L"Oric Atmos - [Nouvelle ROM chargée]");
                return true;
            } else if (e->type == ENTRY_TAP) {
                tape_load_file(full_path);
                memory_reset();
                via_reset();
                ay_reset();
                cpu_reset();
                g_tape.auto_cload_triggered = false; /* Auto-CLOAD au boot BASIC (2.0s) */
                g_browser.active = false;
                wchar_t title[512];
                swprintf(title, 512, L"Oric Atmos - [K7 chargée : %ls]",
                         g_tape.current_name[0] ? (wchar_t *)g_tape.current_name : e->name);
                SetWindowTextW(hwnd, title);
                return true;
            } else if (e->type == ENTRY_DSK) {
                g_microdis_control = 0x00;
                memory_reset();
                via_reset();
                ay_reset();
                cpu_reset();
                g_browser.active = false;
                SetWindowTextW(hwnd, L"Oric Atmos - [Disque chargé]");
                return true;
            }
            return true;
        }
    }

    return false;
}
