#include "memory.h"
#include "via.h"

uint8_t g_ram[65536] = {0};
uint8_t g_rom[16384] = {0};
bool g_rom_loaded = false;

uint8_t g_microdis_rom[8192] = {0};
bool g_microdis_loaded = false;
uint8_t g_microdis_control = 0x04; /* Microdisc: 0x04 = ROM désactivée par défaut */

void memory_reset(void) {
    memset(g_ram, 0, sizeof(g_ram));
    g_microdis_control = 0x04;
}

bool oric_load_rom(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return false;

    size_t n = fread(g_rom, 1, 16384, f);
    fclose(f);

    if (n == 16384) {
        g_rom_loaded = true;
        /* Pré-remplir la police en RAM à 0xB400 à partir de la ROM chargée */
        memcpy(&g_ram[0xB400 + 32 * 8], &g_rom[0xFC78 - 0xC000], 768);
        debug_log(L"ROM chargée : %ls (16384 octets)", path);
        return true;
    } else {
        debug_log(L"Erreur : Taille ROM incorrecte (%u octets)", (unsigned)n);
        return false;
    }
}

bool oric_load_microdis_rom(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return false;

    size_t n = fread(g_microdis_rom, 1, 8192, f);
    fclose(f);

    if (n == 8192) {
        g_microdis_loaded = true;
        debug_log(L"Microdisc ROM chargée : %ls (8192 octets à 0xE000)", path);
        return true;
    } else {
        debug_log(L"Erreur : Taille Microdisc ROM incorrecte (%u octets)", (unsigned)n);
        return false;
    }
}

uint8_t mem_read(uint16_t addr) {
    /* Contrôleur Microdisc FDC registres (0x0310 - 0x0318) */
    if ((addr & 0xFFF8) == 0x0310) {
        uint8_t reg = addr & 0x07;
        switch (reg) {
            case 0: return 0x80; /* FDC Status: ready */
            case 1: return 0;    /* Track */
            case 2: return 1;    /* Sector */
            case 3: return 0;    /* Data */
            case 4: return g_microdis_control;
            default: return 0xFF;
        }
    }

    /* Registres VIA 6522 (0x0300 - 0x030F) */
    if ((addr & 0xFF00) == 0x0300) {
        if ((addr & 0xFFF0) == 0x0300) {
            return via_read(addr & 0x0F);
        }
        return 0xFF;
    }

    /* Microdisc ROM (0xE000 - 0xFFFF) */
    if (addr >= 0xE000 && g_microdis_loaded) {
        if (!(g_microdis_control & 0x04)) {
            return g_microdis_rom[addr - 0xE000];
        }
    }

    /* Oric Atmos Basic ROM (0xC000 - 0xFFFF) */
    if (addr >= 0xC000) {
        if (g_rom_loaded) {
            return g_rom[addr - 0xC000];
        }
    }

    return g_ram[addr];
}

void mem_write(uint16_t addr, uint8_t val) {
    /* Contrôleur Microdisc */
    if ((addr & 0xFFF8) == 0x0310) {
        uint8_t reg = addr & 0x07;
        if (reg == 4) {
            g_microdis_control = val;
        }
        return;
    }

    /* Registres VIA 6522 */
    if ((addr & 0xFF00) == 0x0300) {
        if ((addr & 0xFFF0) == 0x0300) {
            via_write(addr & 0x0F, val);
        }
        return;
    }

    /* Écriture en RAM */
    g_ram[addr] = val;
}
