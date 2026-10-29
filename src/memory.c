#include "memory.h"
#include "via.h"
#include "video.h"
#include "fdc.h"

uint8_t g_ram[65536] = {0};
uint8_t g_rom[16384] = {0};
bool g_rom_loaded = false;

uint8_t g_microdis_rom[8192] = {0};
bool g_microdis_loaded = false;
bool g_romdis = false;
bool g_microdis_eprom = false;

bool rom_has_atmos_font(void) {
    if (!g_rom_loaded) return false;
    /* Vérifier la signature du caractère '!' (code 33) à 0xFC78 + 8 */
    const uint8_t *f = &g_rom[0xFC78 - 0xC000];
    return (f[8 + 1] == 0x08 && f[8 + 2] == 0x08 && f[8 + 6] == 0x08);
}

void memory_reset(void) {
    memset(g_ram, 0, sizeof(g_ram));
    g_hires_mode = false;

    if (g_microdisc_active) {
        g_romdis = true;
        g_microdis_eprom = true;
    } else {
        g_romdis = false;
        g_microdis_eprom = false;
    }

    /* Toujours garantir une police valide en RAM à 0xB400 */
    memcpy(&g_ram[0xB400 + 32 * 8], ORIC_FONT_STANDARD, sizeof(ORIC_FONT_STANDARD));
    if (rom_has_atmos_font()) {
        memcpy(&g_ram[0xB400 + 32 * 8], &g_rom[0xFC78 - 0xC000], sizeof(ORIC_FONT_STANDARD));
    }
}

bool oric_load_rom(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return false;

    size_t n = fread(g_rom, 1, 16384, f);
    fclose(f);

    if (n == 16384) {
        g_rom_loaded = true;
        /* Pré-remplir la police en RAM à 0xB400 avec la police Atmos si présente, sinon police standard */
        if (rom_has_atmos_font()) {
            memcpy(&g_ram[0xB400 + 32 * 8], &g_rom[0xFC78 - 0xC000], sizeof(ORIC_FONT_STANDARD));
        } else {
            memcpy(&g_ram[0xB400 + 32 * 8], ORIC_FONT_STANDARD, sizeof(ORIC_FONT_STANDARD));
        }
        debug_log(L"ROM chargée : %ls (16384 octets, Atmos font=%d)", path, rom_has_atmos_font());
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
    if (g_microdisc_active) {
        /* Registres I/O sur la plage 0x0300 - 0x03FF */
        if ((addr & 0xFF00) == 0x0300) {
            if (addr >= 0x0310 && addr <= 0x031F) {
                return fdc_read(addr);
            }
            if (addr >= 0x0300 && addr <= 0x030F) {
                return via_read(addr & 0x0F);
            }
            return 0xFF;
        }

        /* Routage mémoire haute (0xC000 - 0xFFFF) avec Microdisc */
        if (addr >= 0xC000) {
            /* 1. Si le bit 1 de 0x0314 est 1 (g_romdis == false), la ROM BASIC Atmos interne est active sur tout 0xC000-0xFFFF */
            if (!g_romdis && g_rom_loaded) {
                return g_rom[addr - 0xC000];
            }
            /* 2. Si le bit 1 de 0x0314 est 0 (ROM BASIC désactivée, ROMDIS actif) :
             *    Si le bit 7 de 0x0314 est 0 (g_microdis_eprom == true), l'EPROM Microdisc 8 Ko est active à 0xE000-0xFFFF */
            if (addr >= 0xE000 && g_microdis_loaded && g_microdis_eprom) {
                return g_microdis_rom[addr - 0xE000];
            }
            /* 3. Sinon (ou sur 0xC000-0xDFFF), c'est la RAM d'overlay 64 Ko */
            return g_ram[addr];
        }

        return g_ram[addr];
    }

    /* Mode Oric Atmos standard sans unité de disquette */
    if ((addr & 0xFF00) == 0x0300) {
        if ((addr & 0xFFF0) == 0x0300) {
            return via_read(addr & 0x0F);
        }
        return 0xFF;
    }

    if (addr >= 0xC000 && g_rom_loaded) {
        return g_rom[addr - 0xC000];
    }

    return g_ram[addr];
}

void mem_write(uint16_t addr, uint8_t val) {
    if (g_microdisc_active) {
        /* Registres I/O 0x0300 - 0x03FF */
        if ((addr & 0xFF00) == 0x0300) {
            if (addr >= 0x0310 && addr <= 0x031F) {
                fdc_write(addr, val);
                return;
            }
            if (addr >= 0x0300 && addr <= 0x030F) {
                via_write(addr & 0x0F, val);
                return;
            }
            return;
        }

        /* Sur l'Oric avec Microdisc, toutes les écritures en mémoire haute (0xC000-0xFFFF)
         * écrivent toujours dans la RAM dynamique sous-jacente (Overlay RAM), car les puces
         * ROM et EPROM ne possèdent pas de broche Write-Enable et ignorent les écritures.
         * Cela permet à Sedoric et au BIOS d'initialiser et de charger l'OS dans l'overlay RAM. */
        g_ram[addr] = val;
        return;
    }

    /* Mode Oric Atmos standard */
    if ((addr & 0xFF00) == 0x0300) {
        if ((addr & 0xFFF0) == 0x0300) {
            via_write(addr & 0x0F, val);
        }
        return;
    }

    if (addr >= 0xC000) {
        return; /* ROM BASIC protégée */
    }

    g_ram[addr] = val;
}
