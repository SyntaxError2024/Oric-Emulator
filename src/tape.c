#include "tape.h"
#include "keyboard.h"
#include "via.h"
#include "cpu.h"

TapeState g_tape;

void tape_init(void) {
    memset(&g_tape, 0, sizeof(g_tape));
}

void tape_reset(void) {
    g_tape.pulse_pos = 0;
    g_tape.bit_cycles_left = 350;
    g_tape.last_bit_t2h = 0;
    /* Verrouillage permanent : un reset matériel (F5) ne doit JAMAIS réinjecter cload"" */
    g_tape.auto_cload_triggered = true;
}

static void add_pulse_bit(uint8_t **buf, size_t *count, size_t *cap, uint8_t bit) {
    if (*count >= *cap) {
        *cap = (*cap == 0) ? 65536 : (*cap * 2);
        *buf = (uint8_t *)realloc(*buf, *cap);
    }
    (*buf)[(*count)++] = bit ? 1 : 0;
}

static void add_pulse_byte(uint8_t **buf, size_t *count, size_t *cap, uint8_t b) {
    /* 1. Bit de start (0 / impulsion longue 650 cycles) */
    add_pulse_bit(buf, count, cap, 0);

    /* 2. 8 bits de données (poids faible en premier) */
    int ones = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t bit = (b >> i) & 1;
        ones += bit;
        add_pulse_bit(buf, count, cap, bit);
    }

    /* 3. Bit de parité impaire : nombre total de 1 (données + parité) est toujours impair */
    uint8_t par = (ones ^ 1) & 1;
    add_pulse_bit(buf, count, cap, par);

    /* 4. 4 bits de stop (1 / impulsion courte 350 cycles) */
    for (int i = 0; i < 4; i++) {
        add_pulse_bit(buf, count, cap, 1);
    }
}

static bool tape_load_file_internal(const wchar_t *filename, bool quiet) {
    if (!filename || filename[0] == 0) return false;

    FILE *f = _wfopen(filename, L"rb");
    if (!f) {
        wchar_t alt[MAX_PATH];
        swprintf(alt, MAX_PATH, L"./tapes/%ls", filename);
        f = _wfopen(alt, L"rb");
        if (!f) {
            swprintf(alt, MAX_PATH, L"../tapes/%ls", filename);
            f = _wfopen(alt, L"rb");
        }
        if (!f) {
            const wchar_t *bs = wcsrchr(filename, L'\\');
            const wchar_t *fs = wcsrchr(filename, L'/');
            const wchar_t *base = bs ? (fs && fs > bs ? fs + 1 : bs + 1) : (fs ? fs + 1 : NULL);
            if (base) {
                swprintf(alt, MAX_PATH, L"./tapes/%ls", base);
                f = _wfopen(alt, L"rb");
                if (!f) {
                    swprintf(alt, MAX_PATH, L"../tapes/%ls", base);
                    f = _wfopen(alt, L"rb");
                }
            }
        }
    }

    if (!f) {
        if (!quiet) {
            wchar_t msg[512];
            swprintf(msg, 512, L"Impossible d'ouvrir le fichier .TAP :\n%ls", filename);
            MessageBoxW(NULL, msg, L"Erreur fichier", MB_OK | MB_ICONERROR);
        }
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t fsize = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *raw_buf = (uint8_t *)malloc(fsize);
    if (!raw_buf) {
        fclose(f);
        return false;
    }

    fread(raw_buf, 1, fsize, f);
    fclose(f);

    debug_log(L"Fichier .TAP chargé : %ls (Taille: %u octets)", filename, (unsigned)fsize);

    /* Libération de l'ancien flux d'impulsions */
    if (g_tape.pulse_buf) {
        free(g_tape.pulse_buf);
        g_tape.pulse_buf = NULL;
    }

    /* Construction du flux d'impulsions conforme au décodage matériel Oric Atmos */
    size_t cap = (fsize + 1024) * 14 + 30000;
    size_t count = 0;
    uint8_t *pulses = (uint8_t *)malloc(cap);
    if (!pulses) {
        free(raw_buf);
        return false;
    }

    memset(g_tape.current_name, 0, sizeof(g_tape.current_name));
    bool first_name_extracted = false;

    size_t ptr = 0;
    while (ptr < fsize) {
        /* Recherche de la séquence de synchronisation (0x16 ... 0x24) */
        while (ptr < fsize && raw_buf[ptr] != 0x16) ptr++;
        if (ptr >= fsize) break;
        while (ptr < fsize && raw_buf[ptr] == 0x16) ptr++;
        if (ptr >= fsize) break;
        if (raw_buf[ptr] != 0x24) {
            ptr++;
            continue;
        }
        ptr++; /* Saut du marqueur 0x24 */

        if (ptr + 9 > fsize) break;
        uint8_t hdr[9];
        memcpy(hdr, &raw_buf[ptr], 9);
        ptr += 9;

        /* Extraction du nom du bloc jusqu'à 0x00 */
        char block_name[32] = {0};
        int bni = 0;
        while (ptr < fsize && raw_buf[ptr] != 0) {
            if (bni < 31) block_name[bni++] = (char)raw_buf[ptr];
            ptr++;
        }
        if (ptr < fsize && raw_buf[ptr] == 0) {
            ptr++;
        }

        if (!first_name_extracted) {
            ansi_to_wide(block_name, (wchar_t *)g_tape.current_name, 32);
            debug_log(L"Nom K7 détecté : \"%ls\"", (wchar_t *)g_tape.current_name);
            first_name_extracted = true;
        }

        /* Calcul exact de la longueur des données depuis l'en-tête (adresses de début et fin) */
        uint16_t end_addr   = (uint16_t)((hdr[4] << 8) | hdr[5]);
        uint16_t start_addr = (uint16_t)((hdr[6] << 8) | hdr[7]);
        size_t data_len = 0;
        if (end_addr >= start_addr) {
            data_len = (size_t)(end_addr - start_addr + 1);
        }

        /* 1. Tonalité d'amorce (200 bits 1) */
        for (int i = 0; i < 200; i++) {
            add_pulse_bit(&pulses, &count, &cap, 1);
        }

        /* 2. 16 octets 0x16 pour synchronisation infaillible de la ROM */
        for (int i = 0; i < 16; i++) {
            add_pulse_byte(&pulses, &count, &cap, 0x16);
        }

        /* 3. Marqueur 0x24 */
        add_pulse_byte(&pulses, &count, &cap, 0x24);

        /* 4. Les 9 octets d'en-tête */
        for (int i = 0; i < 9; i++) {
            add_pulse_byte(&pulses, &count, &cap, hdr[i]);
        }

        /* 5. Le nom du bloc terminé par 0x00 */
        for (int i = 0; i < bni; i++) {
            add_pulse_byte(&pulses, &count, &cap, (uint8_t)block_name[i]);
        }
        add_pulse_byte(&pulses, &count, &cap, 0x00);

        /* 6. Pause inter-bloc (200 bits 1) : laisse le temps à la ROM d'afficher le nom sans manquer de données */
        for (int i = 0; i < 200; i++) {
            add_pulse_bit(&pulses, &count, &cap, 1);
        }

        /* 7. Exactement data_len octets de données (lues intégralement sans coupure prématurée) */
        for (size_t i = 0; i < data_len && ptr < fsize; i++) {
            add_pulse_byte(&pulses, &count, &cap, raw_buf[ptr++]);
        }

        /* 8. Pause de fin de bloc (100 bits 1) */
        for (int i = 0; i < 100; i++) {
            add_pulse_bit(&pulses, &count, &cap, 1);
        }
    }

    free(raw_buf);

    g_tape.pulse_buf = pulses;
    g_tape.pulse_count = count;
    g_tape.pulse_pos = 0;
    g_tape.bit_cycles_left = 350;
    g_tape.last_bit_t2h = 0;
    g_tape.loaded = true;

    debug_log(L"Flux K7 matériel généré avec succès (%u impulsions)", (unsigned)count);

    /* Enclenchement de CLOAD "" : si la machine a déjà démarré (boot terminé après 2.0s),
     * on saisit immédiatement. Sinon, cpu_step() s'en chargera dès que le BASIC est prêt. */
    if (g_cpu.cycles >= 2000000) {
        g_tape.auto_cload_triggered = true;
        typein_queue("\rcload\"\"\r");
        debug_log(L"Auto-CLOAD enclenché : saisie de '\\rcload\"\"\\r'");
    } else {
        g_tape.auto_cload_triggered = false;
        debug_log(L"K7 préchargée au démarrage : Auto-CLOAD différé après boot BASIC (2.0s)");
    }

    return true;
}

bool tape_load_file(const wchar_t *filename) {
    return tape_load_file_internal(filename, false);
}

bool tape_load_file_quiet(const wchar_t *filename) {
    return tape_load_file_internal(filename, true);
}

void tape_step(uint32_t cycles) {
    if (!g_tape.loaded || !g_tape.pulse_buf || g_tape.pulse_count == 0) return;

    /* Le relais moteur de la cassette est commandé par le bit PB6 du VIA */
    bool motor_on = (g_via.orb & 0x40) != 0;
    if (!motor_on) return;

    /* Fin de bande : arrêt propre du flux K7 sans reboucler intempestivement */
    if (g_tape.pulse_pos >= g_tape.pulse_count) {
        return;
    }

    g_tape.bit_cycles_left -= (int32_t)cycles;
    while (g_tape.bit_cycles_left <= 0 && g_tape.pulse_pos < g_tape.pulse_count) {
        /* Si l'interruption CB1 précédente n'a pas encore été lue et acquittée par la ROM via LDA $0300,
         * on patiente sans faire avancer la bande afin de ne perdre aucune impulsion */
        if (g_via.ifr & 0x10) {
            g_tape.bit_cycles_left = 0;
            break;
        }

        /* Transition active sur la broche CB1 du VIA 6522 */
        g_via.ifr |= 0x10;

        uint8_t bit = g_tape.pulse_buf[g_tape.pulse_pos++];
        if (bit) {
            /* Bit 1 (période courte ~350 cycles) :
             * Timer 2 High = 0xFE -> dans la routine ROM ($E731 CMP #$FE)
             * la retenue CARRY est mise à 1. */
            g_via.t2c = 0xFE50;
            g_tape.last_bit_t2h = 0xFE;
            g_tape.bit_cycles_left += 350;
        } else {
            /* Bit 0 (période longue ~650 cycles) :
             * Timer 2 High = 0xFD -> dans la routine ROM ($E731 CMP #$FE)
             * la retenue CARRY est mise à 0. */
            g_via.t2c = 0xFD50;
            g_tape.last_bit_t2h = 0xFD;
            g_tape.bit_cycles_left += 650;
        }
    }
}
