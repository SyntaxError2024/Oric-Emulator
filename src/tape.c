#include "tape.h"
#include "keyboard.h"
#include "via.h"

TapeState g_tape;

void tape_init(void) {
    memset(&g_tape, 0, sizeof(g_tape));
}

void tape_reset(void) {
    g_tape.byte_pos = 0;
    g_tape.bit_in_byte = 0;
    g_tape.started = false;
    g_tape.auto_cload_triggered = false;
}

bool tape_load_file(const wchar_t *filename) {
    FILE *f = _wfopen(filename, L"rb");
    if (!f) {
        wchar_t msg[512];
        swprintf(msg, 512, L"Impossible d'ouvrir le fichier .TAP :\n%ls", filename);
        MessageBoxW(NULL, msg, L"Erreur fichier", MB_OK | MB_ICONERROR);
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)malloc(fsize);
    if (!buf) {
        fclose(f);
        return false;
    }
    fread(buf, 1, fsize, f);
    fclose(f);

    debug_log(L"Fichier .TAP chargé : %ls (Taille: %u octets)", filename, (unsigned)fsize);

    if (g_tape.data) {
        free(g_tape.data);
    }
    g_tape.data = buf;
    g_tape.size = fsize;
    g_tape.byte_pos = 0;
    g_tape.bit_in_byte = 0;
    g_tape.bit_cycles_left = 350;
    g_tape.active = true;
    g_tape.auto_cload_triggered = false;
    g_tape.started = false;
    memset(g_tape.current_name, 0, sizeof(g_tape.current_name));

    /* Extraction du nom du premier bloc */
    size_t ptr = 0;
    while (ptr < fsize && buf[ptr] != 0x24) ptr++;
    if (ptr < fsize) {
        ptr++;
        if (ptr + 9 <= fsize) {
            ptr += 9;
            char bname[32] = {0};
            int ni = 0;
            while (ptr < fsize && buf[ptr] != 0 && ni < 31) {
                bname[ni++] = (char)buf[ptr++];
            }
            ansi_to_wide(bname, (wchar_t *)g_tape.current_name, 32);
            debug_log(L"Programme K7 détecté : \"%ls\"", (wchar_t *)g_tape.current_name);
        }
    }

    /* Injection automatique de CLOAD */
    typein_queue("CLOAD\r");
    debug_log(L"Auto-CLOAD enclenché : saisie de 'CLOAD\\r'");

    return true;
}

bool tape_hook_byte(uint8_t *out_byte) {
    if (!g_tape.active || !g_tape.data || g_tape.byte_pos >= g_tape.size) {
        return false;
    }
    *out_byte = g_tape.data[g_tape.byte_pos++];
    if (g_tape.byte_pos >= g_tape.size) {
        debug_log(L"Fin de la cassette atteinte (%u octets transférés)", (unsigned)g_tape.size);
        g_tape.active = false;
        g_tape.started = false;
    }
    return true;
}

bool tape_hook_sync(void) {
    if (!g_tape.active || !g_tape.data || g_tape.byte_pos >= g_tape.size) {
        return false;
    }
    /* Recherche de la prochaine synchro 0x16 dans le flux */
    while (g_tape.byte_pos < g_tape.size && g_tape.data[g_tape.byte_pos] != 0x16) {
        g_tape.byte_pos++;
    }
    if (g_tape.byte_pos < g_tape.size && g_tape.data[g_tape.byte_pos] == 0x16) {
        return true;
    }
    return false;
}

bool tape_hook_bit(uint8_t *out_bit) {
    if (!g_tape.active || !g_tape.data || g_tape.byte_pos >= g_tape.size) {
        return false;
    }
    *out_bit = (g_tape.data[g_tape.byte_pos] >> g_tape.bit_in_byte) & 1;
    g_tape.bit_in_byte++;
    if (g_tape.bit_in_byte >= 8) {
        g_tape.bit_in_byte = 0;
        g_tape.byte_pos++;
        if (g_tape.byte_pos >= g_tape.size) {
            g_tape.active = false;
            g_tape.started = false;
        }
    }
    return true;
}

void tape_step(uint32_t cycles) {
    if (!g_tape.active || !g_tape.data || g_tape.size == 0) return;

    bool motor_on = (g_via.orb & 0x40) != 0;
    if (!motor_on && !g_tape.started) return;

    if (!g_tape.started && motor_on) {
        g_tape.started = true;
        g_tape.bit_cycles_left = 350;
        debug_log(L"Moteur cassette activé (Relais PB6)");
    }

    if (g_tape.byte_pos >= g_tape.size) return;

    if (g_tape.bit_cycles_left <= cycles) {
        /* Impulsion CB1 pour signaux matériels */
        g_via.ifr |= 0x10;
        g_tape.bit_cycles_left = 400;
    } else {
        g_tape.bit_cycles_left -= cycles;
    }
}
