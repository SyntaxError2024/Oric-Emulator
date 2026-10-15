/*
 * ============================================================================
 * ORIC ATMOS - ÉMULATEUR WIN32 PUR (C UNICODE / MinGW64 / GDI)
 * ============================================================================
 * - Aucun framework tiers, aucune bibliothèque externe à Windows.
 * - Compilation directe avec MinGW64 :
 *     gcc -O3 oric_win32.c -o oric_atmos.exe -municode -lgdi32 -lwinmm -lshell32
 * - Conforme aux exigences :
 *     - Langage C pur pour Windows en Unicode complet (wWinMain / wmain, wchar_t, MessageBoxW, etc.).
 *     - Fichier source encodé en UTF-8.
 *     - Fichiers batch (.bat) encodés en Windows-1252.
 *     - Console de débogage active avec affichage en direct des événements
 *       (touches pressées, fichiers TAP/ROM chargés, mode écran TEXT/HIRES, etc.).
 *     - Glisser-déposer de fichiers .TAP et .ROM via WM_DROPFILES (Drag & Drop).
 *     - Démarrage automatique instantané (Turbo Tape) pour jeux machine et BASIC.
 *     - Émulation fidèle : MOS 6502 (BCD, IRQ, NMI), VIA 6522 (Timers 1 & 2),
 *       AY-3-8912 (son waveOut et matrice clavier 8x8), ULA vidéo (TEXT 40x28,
 *       HIRES 240x200 + 3 lignes de texte, attributs série Oric).
 * ============================================================================
 */

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

/* Palette RVB standard Oric */
static const uint32_t PALETTE[8] = {
    0x000000, /* 0: Noir */
    0xFF0000, /* 1: Rouge */
    0x00FF00, /* 2: Vert */
    0xFFFF00, /* 3: Jaune */
    0x0000FF, /* 4: Bleu */
    0xFF00FF, /* 5: Magenta */
    0x00FFFF, /* 6: Cyan */
    0xFFFFFF  /* 7: Blanc */
};

static const uint8_t ORIC_FONT_STANDARD[768] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00,
    0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x14, 0x3E, 0x14, 0x3E, 0x14, 0x14, 0x00,
    0x08, 0x1E, 0x28, 0x1C, 0x0A, 0x3C, 0x08, 0x00, 0x30, 0x32, 0x04, 0x08, 0x10, 0x26, 0x06, 0x00,
    0x10, 0x28, 0x28, 0x10, 0x2A, 0x24, 0x1A, 0x00, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00,
    0x08, 0x2A, 0x1C, 0x08, 0x1C, 0x2A, 0x08, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x10, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x00,
    0x1C, 0x22, 0x26, 0x2A, 0x32, 0x22, 0x1C, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00,
    0x1C, 0x22, 0x02, 0x04, 0x08, 0x10, 0x3E, 0x00, 0x3E, 0x02, 0x04, 0x0C, 0x02, 0x22, 0x1C, 0x00,
    0x04, 0x0C, 0x14, 0x24, 0x3E, 0x04, 0x04, 0x00, 0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C, 0x00,
    0x0C, 0x10, 0x20, 0x3C, 0x22, 0x22, 0x1C, 0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00,
    0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C, 0x00, 0x1C, 0x22, 0x22, 0x1E, 0x02, 0x04, 0x18, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x08, 0x10,
    0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00, 0x00,
    0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x1C, 0x22, 0x04, 0x08, 0x08, 0x00, 0x08, 0x00,
    0x1C, 0x22, 0x2A, 0x2E, 0x2C, 0x20, 0x1E, 0x00, 0x08, 0x14, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x00,
    0x3C, 0x22, 0x22, 0x3C, 0x22, 0x22, 0x3C, 0x00, 0x1C, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1C, 0x00,
    0x3C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3C, 0x00, 0x3E, 0x20, 0x20, 0x3C, 0x20, 0x20, 0x3E, 0x00,
    0x3E, 0x20, 0x20, 0x3C, 0x20, 0x20, 0x20, 0x00, 0x1E, 0x20, 0x20, 0x20, 0x26, 0x22, 0x1E, 0x00,
    0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x22, 0x1C, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3E, 0x00, 0x22, 0x36, 0x2A, 0x2A, 0x22, 0x22, 0x22, 0x00,
    0x22, 0x22, 0x32, 0x2A, 0x26, 0x22, 0x22, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00,
    0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x20, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x2A, 0x24, 0x1A, 0x00,
    0x3C, 0x22, 0x22, 0x3C, 0x28, 0x24, 0x22, 0x00, 0x1C, 0x22, 0x20, 0x1C, 0x02, 0x22, 0x1C, 0x00,
    0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x14, 0x08, 0x00, 0x22, 0x22, 0x22, 0x2A, 0x2A, 0x36, 0x22, 0x00,
    0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00,
    0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3E, 0x00, 0x1E, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1E, 0x00,
    0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00, 0x3C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x3C, 0x00,
    0x08, 0x14, 0x2A, 0x08, 0x08, 0x08, 0x08, 0x00, 0x0E, 0x10, 0x10, 0x10, 0x3C, 0x10, 0x3E, 0x00,
    0x0C, 0x12, 0x2D, 0x29, 0x29, 0x2D, 0x12, 0x0C, 0x00, 0x00, 0x1C, 0x02, 0x1E, 0x22, 0x1E, 0x00,
    0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x3C, 0x00, 0x00, 0x00, 0x1E, 0x20, 0x20, 0x20, 0x1E, 0x00,
    0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x3E, 0x20, 0x1E, 0x00,
    0x0C, 0x12, 0x10, 0x3C, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1E, 0x02, 0x1C,
    0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x00, 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1C, 0x00,
    0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x24, 0x18, 0x20, 0x20, 0x22, 0x24, 0x38, 0x24, 0x22, 0x00,
    0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00, 0x00, 0x36, 0x2A, 0x2A, 0x2A, 0x22, 0x00,
    0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00,
    0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x00, 0x00, 0x1E, 0x22, 0x22, 0x1E, 0x02, 0x02,
    0x00, 0x00, 0x2E, 0x30, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x00,
    0x10, 0x10, 0x3C, 0x10, 0x10, 0x12, 0x0C, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x26, 0x1A, 0x00,
    0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x08, 0x00, 0x00, 0x00, 0x22, 0x22, 0x2A, 0x2A, 0x36, 0x00,
    0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x1C,
    0x00, 0x00, 0x3E, 0x04, 0x08, 0x10, 0x3E, 0x00, 0x0E, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0E, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38, 0x0C, 0x0C, 0x06, 0x0C, 0x0C, 0x38, 0x00,
    0x2A, 0x15, 0x2A, 0x15, 0x2A, 0x15, 0x2A, 0x15, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F
};


/* Table des glyphes accentués français de la ROM 110fr (8 glyphes de 8 octets) */
static const uint8_t ORIC_FRENCH_GLYPHS[8][8] = {
    { 0x22, 0x22, 0x1E, 0x00, 0x00, 0x00, 0x1C, 0x22 }, /* 0: à */
    { 0x3E, 0x20, 0x1E, 0x00, 0x0C, 0x12, 0x10, 0x3C }, /* 1: é */
    { 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x1C, 0x22 }, /* 2: è */
    { 0x22, 0x1E, 0x02, 0x1C, 0x20, 0x20, 0x3C, 0x22 }, /* 3: ê */
    { 0x22, 0x22, 0x22, 0x00, 0x08, 0x00, 0x18, 0x08 }, /* 4: ç */
    { 0x08, 0x08, 0x1C, 0x00, 0x04, 0x00, 0x0C, 0x04 }, /* 5: ù */
    { 0x04, 0x04, 0x24, 0x18, 0x20, 0x20, 0x22, 0x24 }, /* 6: ô */
    { 0x38, 0x24, 0x22, 0x00, 0x18, 0x08, 0x08, 0x08 }  /* 7: î */
};

/* --- Mémoire Oric --- */
static uint8_t g_ram[65536];
static uint8_t g_rom[16384];
static bool g_rom_loaded = false;

/* --- ULA & Affichage --- */
static uint32_t g_framebuffer[SCREEN_W * SCREEN_H];
static BITMAPINFO g_bmi;
static bool g_hires_mode = false;
static int g_frame_counter = 0;

/* --- Matrice Clavier Oric (8x8) --- */
/*
 * Colonnes sélectionnées par le port IO du AY-3-8912 (Reg 14, logique inversée)
 * Lignes lues via le multiplexeur 4051 piloté par VIA Port B (PB0..PB2).
 * Résultat lu sur VIA Port B bit 3 (1 = enfoncé, 0 = relâché).
 */
static uint8_t g_key_matrix[8];

/* Buffer de fichier .TAP */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t pos;
    char current_name[32];
    uint16_t start_addr;
    uint16_t end_addr;
    uint8_t file_type;
    uint8_t autorun;
    bool active;
} TapeState;

static TapeState g_tape;

/* --- 6502 CPU --- */
typedef struct {
    uint16_t pc;
    uint8_t a, x, y, sp, p;
    uint64_t cycles;
    bool irq_pending;
    bool nmi_pending;
} CPU6502;

static CPU6502 g_cpu;

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

static inline void cpu_set_nz(uint8_t val) {
    g_cpu.p &= ~(FLAG_N | FLAG_Z);
    if (!val) g_cpu.p |= FLAG_Z;
    if (val & 0x80) g_cpu.p |= FLAG_N;
}

/* --- 6522 VIA --- */
typedef struct {
    uint8_t ora, orb;
    uint8_t ddra, ddrb;
    uint16_t t1c, t1l;
    uint16_t t2c, t2l;
    uint8_t sr;
    uint8_t acr;
    uint8_t pcr;
    uint8_t ifr;
    uint8_t ier;
    bool t1_active;
    bool t2_active;
} VIA6522;

static VIA6522 g_via;

/* --- AY-3-8912 PSG (Son & Port IO) --- */
typedef struct {
    uint8_t regs[16];
    uint8_t selected_reg;
    uint32_t tone_period[3];
    uint32_t tone_counter[3];
    uint8_t tone_output[3];
    uint32_t noise_period;
    uint32_t noise_counter;
    uint32_t noise_lfsr;
    uint8_t noise_output;
    uint32_t env_period;
    uint32_t env_counter;
    uint8_t env_step;
    uint8_t env_volume;
    HWAVEOUT hWaveOut;
    WAVEHDR waveHdr[2];
    short audioBuffer[2][1024];
    int currentBuffer;
    bool audio_enabled;
} AY38912;

static AY38912 g_ay;

/* Prototypes */
static void debug_init(void);
static void debug_log(const wchar_t *format, ...);
static void cpu_reset(void);
static void cpu_step(void);
static uint8_t mem_read(uint16_t addr);
static void mem_write(uint16_t addr, uint8_t val);
static void via_reset(void);
static void via_step(uint32_t cycles);
static void ay_reset(void);
static void ay_write_reg(uint8_t reg, uint8_t val);
static void ay_check_control(void);
static void oric_load_rom(const wchar_t *path);
static void oric_inject_tap_and_run(const wchar_t *filename);
static void keyboard_handle_vk(UINT vk, bool pressed);
static void audio_init(HWND hwnd);
static void audio_update(void);
static void audio_shutdown(void);
static void video_render_frame(void);

/* ============================================================================
 * CONSOLE DE DÉBOGAGE
 * ============================================================================ */
static void debug_init(void) {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }
    FILE *fpOut = NULL, *fpErr = NULL;
    _wfreopen_s(&fpOut, L"CONOUT$", L"w", stdout);
    _wfreopen_s(&fpErr, L"CONOUT$", L"w", stderr);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"Oric Atmos Win32 - Console de Débogage");

    wprintf(L"===============================================================\n");
    wprintf(L"  Émulateur Oric Atmos Win32 (C Unicode / MinGW64)\n");
    wprintf(L"  Console de débogage active\n");
    wprintf(L"===============================================================\n\n");
}

static void debug_log(const wchar_t *format, ...) {
    va_list args;
    va_start(args, format);
    wprintf(L"[DEBUG] ");
    vwprintf(format, args);
    wprintf(L"\n");
    va_end(args);
    fflush(stdout);
}

/* ============================================================================
 * CONTRÔLE DU BUS AY-3-8912 PAR LE VIA 6522
 * ============================================================================ */
static void ay_check_control(void) {
    bool bc1  = (g_via.pcr & 0x0E) == 0x0E;
    bool bdir = (g_via.pcr & 0xE0) == 0xE0;

    if (bc1 && bdir) {
        g_ay.selected_reg = g_via.ora & 0x0F;
    } else if (!bc1 && bdir) {
        ay_write_reg(g_ay.selected_reg, g_via.ora);
    }
}

/* ============================================================================
 * ACCÈS MÉMOIRE
 * ============================================================================ */
static uint8_t mem_read(uint16_t addr) {
    if ((addr & 0xFF00) == 0x0300) {
        /* Adresses 0x0310..0x03FF non mappées en l'absence de contrôleur disquette (bus flottant = 0xFF) */
        if ((addr & 0xFFF0) != 0x0300) {
            return 0xFF;
        }

        uint8_t reg = addr & 0x0F;
        switch (reg) {
            case 0x00: {
                uint8_t row = g_via.orb & 0x07;
                uint8_t col_mask = (~g_ay.regs[14]) & 0xFF;
                uint8_t key_bit = (g_key_matrix[row] & col_mask) ? 0x08 : 0x00;
                return (g_via.orb & g_via.ddrb) | (key_bit & ~g_via.ddrb);
            }
            case 0x01:
            case 0x0F: {
                bool bc1  = (g_via.pcr & 0x0E) == 0x0E;
                bool bdir = (g_via.pcr & 0xE0) == 0xE0;
                if (bc1 && !bdir) {
                    return g_ay.regs[g_ay.selected_reg & 0x0F];
                }
                return (g_via.ora & g_via.ddra) | (~g_via.ddra & 0xFF);
            }
            case 0x02: return g_via.ddrb;
            case 0x03: return g_via.ddra;
            case 0x04:
                g_via.ifr &= ~0x40;
                return g_via.t1c & 0xFF;
            case 0x05:
                return (g_via.t1c >> 8) & 0xFF;
            case 0x06: return g_via.t1l & 0xFF;
            case 0x07: return (g_via.t1l >> 8) & 0xFF;
            case 0x08:
                g_via.ifr &= ~0x20;
                return g_via.t2c & 0xFF;
            case 0x09: return (g_via.t2c >> 8) & 0xFF;
            case 0x0A: return g_via.sr;
            case 0x0B: return g_via.acr;
            case 0x0C: return g_via.pcr;
            case 0x0D:
                return g_via.ifr | ((g_via.ifr & g_via.ier & 0x7F) ? 0x80 : 0x00);
            case 0x0E: return g_via.ier | 0x80;
        }
    }

    if (addr >= 0xC000) {
        if (g_rom_loaded) {
            return g_rom[addr - 0xC000];
        }
    }

    return g_ram[addr];
}

static void mem_write(uint16_t addr, uint8_t val) {
    if ((addr & 0xFF00) == 0x0300) {
        if ((addr & 0xFFF0) != 0x0300) return;
        uint8_t reg = addr & 0x0F;
        switch (reg) {
            case 0x00:
                g_via.orb = val;
                break;
            case 0x01:
            case 0x0F:
                g_via.ora = val;
                ay_check_control();
                break;
            case 0x02: g_via.ddrb = val; break;
            case 0x03: g_via.ddra = val; break;
            case 0x04: g_via.t1l = (g_via.t1l & 0xFF00) | val; break;
            case 0x05:
                g_via.t1l = (g_via.t1l & 0x00FF) | ((uint16_t)val << 8);
                g_via.t1c = g_via.t1l;
                g_via.ifr &= ~0x40;
                g_via.t1_active = true;
                break;
            case 0x06: g_via.t1l = (g_via.t1l & 0xFF00) | val; break;
            case 0x07: g_via.t1l = (g_via.t1l & 0x00FF) | ((uint16_t)val << 8); break;
            case 0x08: g_via.t2l = (g_via.t2l & 0xFF00) | val; break;
            case 0x09:
                g_via.t2l = (g_via.t2l & 0x00FF) | ((uint16_t)val << 8);
                g_via.t2c = g_via.t2l;
                g_via.ifr &= ~0x20;
                g_via.t2_active = true;
                break;
            case 0x0A: g_via.sr = val; break;
            case 0x0B: g_via.acr = val; break;
            case 0x0C:
                g_via.pcr = val;
                ay_check_control();
                break;
            case 0x0D:
                g_via.ifr &= ~(val & 0x7F);
                break;
            case 0x0E:
                if (val & 0x80) g_via.ier |= (val & 0x7F);
                else g_via.ier &= ~(val & 0x7F);
                break;
        }
        return;
    }

    g_ram[addr] = val;
}

/* ============================================================================
 * ÉMULATION CPU 6502
 * ============================================================================ */
static void cpu_reset(void) {
    g_cpu.a = 0;
    g_cpu.x = 0;
    g_cpu.y = 0;
    g_cpu.sp = 0xFD;
    g_cpu.p = FLAG_U | FLAG_I;
    g_cpu.irq_pending = false;
    g_cpu.nmi_pending = false;
    g_cpu.cycles = 0;
    memset(g_key_matrix, 0, sizeof(g_key_matrix));

    uint8_t lo = mem_read(0xFFFC);
    uint8_t hi = mem_read(0xFFFD);
    g_cpu.pc = lo | ((uint16_t)hi << 8);
    if (g_cpu.pc == 0 || g_cpu.pc == 0xFFFF) {
        g_cpu.pc = 0xF88F;
    }
    debug_log(L"CPU 6502 réinitialisé (PC=0x%04X, SP=0x%02X)", g_cpu.pc, g_cpu.sp);
}

static inline void cpu_push(uint8_t val) {
    mem_write(0x0100 + (g_cpu.sp-- & 0xFF), val);
}

static inline uint8_t cpu_pop(void) {
    return mem_read(0x0100 + (++g_cpu.sp & 0xFF));
}

static void cpu_step(void) {
    if (g_cpu.nmi_pending) {
        g_cpu.nmi_pending = false;
        cpu_push((g_cpu.pc >> 8) & 0xFF);
        cpu_push(g_cpu.pc & 0xFF);
        cpu_push((g_cpu.p | FLAG_U) & ~FLAG_B);
        g_cpu.p |= FLAG_I;
        g_cpu.pc = mem_read(0xFFFA) | ((uint16_t)mem_read(0xFFFB) << 8);
        g_cpu.cycles += 7;
        return;
    }

    if (g_cpu.irq_pending && !(g_cpu.p & FLAG_I)) {
        cpu_push((g_cpu.pc >> 8) & 0xFF);
        cpu_push(g_cpu.pc & 0xFF);
        cpu_push((g_cpu.p | FLAG_U) & ~FLAG_B);
        g_cpu.p |= FLAG_I;
        g_cpu.pc = mem_read(0xFFFE) | ((uint16_t)mem_read(0xFFFF) << 8);
        g_cpu.cycles += 7;
        return;
    }

    uint8_t opcode = mem_read(g_cpu.pc++);
    g_cpu.cycles += 2;

    switch (opcode) {
        case 0x00: {
            g_cpu.pc++;
            cpu_push((g_cpu.pc >> 8) & 0xFF);
            cpu_push(g_cpu.pc & 0xFF);
            cpu_push(g_cpu.p | FLAG_B | FLAG_U);
            g_cpu.p |= FLAG_I;
            g_cpu.pc = mem_read(0xFFFE) | ((uint16_t)mem_read(0xFFFF) << 8);
            g_cpu.cycles += 5;
            break;
        }

        case 0xEA: case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: break;

        case 0xA9: g_cpu.a = mem_read(g_cpu.pc++); cpu_set_nz(g_cpu.a); break;
        case 0xA5: g_cpu.a = mem_read(mem_read(g_cpu.pc++)); cpu_set_nz(g_cpu.a); break;
        case 0xB5: g_cpu.a = mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF); cpu_set_nz(g_cpu.a); break;
        case 0xAD: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.a = mem_read(a); cpu_set_nz(g_cpu.a); break; }
        case 0xBD: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.a = mem_read(a + g_cpu.x); cpu_set_nz(g_cpu.a); break; }
        case 0xB9: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.a = mem_read(a + g_cpu.y); cpu_set_nz(g_cpu.a); break; }
        case 0xA1: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); g_cpu.a = mem_read(a); cpu_set_nz(g_cpu.a); break; }
        case 0xB1: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; g_cpu.a = mem_read(a); cpu_set_nz(g_cpu.a); break; }

        case 0xA2: g_cpu.x = mem_read(g_cpu.pc++); cpu_set_nz(g_cpu.x); break;
        case 0xA6: g_cpu.x = mem_read(mem_read(g_cpu.pc++)); cpu_set_nz(g_cpu.x); break;
        case 0xB6: g_cpu.x = mem_read((mem_read(g_cpu.pc++) + g_cpu.y) & 0xFF); cpu_set_nz(g_cpu.x); break;
        case 0xAE: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.x = mem_read(a); cpu_set_nz(g_cpu.x); break; }
        case 0xBE: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.x = mem_read(a + g_cpu.y); cpu_set_nz(g_cpu.x); break; }

        case 0xA0: g_cpu.y = mem_read(g_cpu.pc++); cpu_set_nz(g_cpu.y); break;
        case 0xA4: g_cpu.y = mem_read(mem_read(g_cpu.pc++)); cpu_set_nz(g_cpu.y); break;
        case 0xB4: g_cpu.y = mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF); cpu_set_nz(g_cpu.y); break;
        case 0xAC: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.y = mem_read(a); cpu_set_nz(g_cpu.y); break; }
        case 0xBC: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.y = mem_read(a + g_cpu.x); cpu_set_nz(g_cpu.y); break; }

        case 0x85: mem_write(mem_read(g_cpu.pc++), g_cpu.a); break;
        case 0x95: mem_write((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF, g_cpu.a); break;
        case 0x8D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); mem_write(a, g_cpu.a); break; }
        case 0x9D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); mem_write(a + g_cpu.x, g_cpu.a); break; }
        case 0x99: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); mem_write(a + g_cpu.y, g_cpu.a); break; }
        case 0x81: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); mem_write(a, g_cpu.a); break; }
        case 0x91: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; mem_write(a, g_cpu.a); break; }

        case 0x86: mem_write(mem_read(g_cpu.pc++), g_cpu.x); break;
        case 0x96: mem_write((mem_read(g_cpu.pc++) + g_cpu.y) & 0xFF, g_cpu.x); break;
        case 0x8E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); mem_write(a, g_cpu.x); break; }

        case 0x84: mem_write(mem_read(g_cpu.pc++), g_cpu.y); break;
        case 0x94: mem_write((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF, g_cpu.y); break;
        case 0x8C: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); mem_write(a, g_cpu.y); break; }

        case 0xAA: g_cpu.x = g_cpu.a; cpu_set_nz(g_cpu.x); break;
        case 0x8A: g_cpu.a = g_cpu.x; cpu_set_nz(g_cpu.a); break;
        case 0xA8: g_cpu.y = g_cpu.a; cpu_set_nz(g_cpu.y); break;
        case 0x98: g_cpu.a = g_cpu.y; cpu_set_nz(g_cpu.a); break;
        case 0xBA: g_cpu.x = g_cpu.sp; cpu_set_nz(g_cpu.x); break;
        case 0x9A: g_cpu.sp = g_cpu.x; break;

        case 0x48: cpu_push(g_cpu.a); break;
        case 0x68: g_cpu.a = cpu_pop(); cpu_set_nz(g_cpu.a); break;
        case 0x08: cpu_push(g_cpu.p | FLAG_B | FLAG_U); break;
        case 0x28: g_cpu.p = (cpu_pop() | FLAG_U) & ~FLAG_B; break;

        case 0x18: g_cpu.p &= ~FLAG_C; break;
        case 0x38: g_cpu.p |= FLAG_C; break;
        case 0x58: g_cpu.p &= ~FLAG_I; break;
        case 0x78: g_cpu.p |= FLAG_I; break;
        case 0xB8: g_cpu.p &= ~FLAG_V; break;
        case 0xD8: g_cpu.p &= ~FLAG_D; break;
        case 0xF8: g_cpu.p |= FLAG_D; break;

        case 0x4C: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); g_cpu.pc = a; break; }
        case 0x6C: {
            uint16_t ptr = mem_read(g_cpu.pc++); ptr |= ((uint16_t)mem_read(g_cpu.pc++) << 8);
            uint16_t hi_ptr = (ptr & 0xFF00) | ((ptr + 1) & 0xFF);
            g_cpu.pc = mem_read(ptr) | ((uint16_t)mem_read(hi_ptr) << 8);
            break;
        }
        case 0x20: {
            uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8);
            uint16_t ret = g_cpu.pc - 1;
            cpu_push((ret >> 8) & 0xFF);
            cpu_push(ret & 0xFF);
            g_cpu.pc = a;
            break;
        }
        case 0x60: {
            uint16_t lo = cpu_pop();
            uint16_t hi = cpu_pop();
            g_cpu.pc = (lo | (hi << 8)) + 1;
            break;
        }
        case 0x40: {
            g_cpu.p = (cpu_pop() | FLAG_U) & ~FLAG_B;
            uint16_t lo = cpu_pop();
            uint16_t hi = cpu_pop();
            g_cpu.pc = lo | (hi << 8);
            break;
        }

        #define BRANCH(cond) { int8_t rel = (int8_t)mem_read(g_cpu.pc++); if (cond) { g_cpu.pc += rel; g_cpu.cycles++; } }
        case 0x10: BRANCH(!(g_cpu.p & FLAG_N)); break;
        case 0x30: BRANCH( (g_cpu.p & FLAG_N)); break;
        case 0x50: BRANCH(!(g_cpu.p & FLAG_V)); break;
        case 0x70: BRANCH( (g_cpu.p & FLAG_V)); break;
        case 0x90: BRANCH(!(g_cpu.p & FLAG_C)); break;
        case 0xB0: BRANCH( (g_cpu.p & FLAG_C)); break;
        case 0xD0: BRANCH(!(g_cpu.p & FLAG_Z)); break;
        case 0xF0: BRANCH( (g_cpu.p & FLAG_Z)); break;

        case 0xE6: { uint8_t a = mem_read(g_cpu.pc++); uint8_t v = mem_read(a) + 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0xF6: { uint8_t a = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint8_t v = mem_read(a) + 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0xEE: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a) + 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0xFE: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a + g_cpu.x) + 1; mem_write(a + g_cpu.x, v); cpu_set_nz(v); break; }
        case 0xC6: { uint8_t a = mem_read(g_cpu.pc++); uint8_t v = mem_read(a) - 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0xD6: { uint8_t a = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint8_t v = mem_read(a) - 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0xCE: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a) - 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0xDE: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a + g_cpu.x) - 1; mem_write(a + g_cpu.x, v); cpu_set_nz(v); break; }
        case 0xE8: g_cpu.x++; cpu_set_nz(g_cpu.x); break;
        case 0xCA: g_cpu.x--; cpu_set_nz(g_cpu.x); break;
        case 0xC8: g_cpu.y++; cpu_set_nz(g_cpu.y); break;
        case 0x88: g_cpu.y--; cpu_set_nz(g_cpu.y); break;

        #define DO_ADC(expr) {             uint8_t val = (expr);             int carry = (g_cpu.p & FLAG_C) ? 1 : 0;             if (g_cpu.p & FLAG_D) {                 int al = (g_cpu.a & 0x0F) + (val & 0x0F) + carry;                 int ah = (g_cpu.a >> 4) + (val >> 4);                 if (al > 9) { al += 6; ah++; }                 if (ah > 9) { ah += 6; }                 g_cpu.p &= ~FLAG_C;                 if (ah > 0x0F) g_cpu.p |= FLAG_C;                 g_cpu.a = ((ah & 0x0F) << 4) | (al & 0x0F);             } else {                 uint16_t sum = g_cpu.a + val + carry;                 g_cpu.p &= ~(FLAG_C | FLAG_V);                 if (sum > 0xFF) g_cpu.p |= FLAG_C;                 if (~(g_cpu.a ^ val) & (g_cpu.a ^ sum) & 0x80) g_cpu.p |= FLAG_V;                 g_cpu.a = (uint8_t)sum;             }             cpu_set_nz(g_cpu.a);         }
        case 0x69: DO_ADC(mem_read(g_cpu.pc++)); break;
        case 0x65: DO_ADC(mem_read(mem_read(g_cpu.pc++))); break;
        case 0x75: DO_ADC(mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0x6D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ADC(mem_read(a)); break; }
        case 0x7D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ADC(mem_read(a + g_cpu.x)); break; }
        case 0x79: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ADC(mem_read(a + g_cpu.y)); break; }
        case 0x61: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_ADC(mem_read(a)); break; }
        case 0x71: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_ADC(mem_read(a)); break; }

        #define DO_SBC(expr) {             uint8_t val = (expr);             int borrow = (g_cpu.p & FLAG_C) ? 0 : 1;             int diff = g_cpu.a - val - borrow;             if (g_cpu.p & FLAG_D) {                 int al = (g_cpu.a & 0x0F) - (val & 0x0F) - borrow;                 int ah = (g_cpu.a >> 4) - (val >> 4);                 if (al < 0) { al -= 6; ah--; }                 if (ah < 0) { ah -= 6; }                 g_cpu.p &= ~FLAG_C;                 if (diff >= 0) g_cpu.p |= FLAG_C;                 g_cpu.a = ((ah & 0x0F) << 4) | (al & 0x0F);             } else {                 g_cpu.p &= ~(FLAG_C | FLAG_V);                 if (diff >= 0) g_cpu.p |= FLAG_C;                 if ((g_cpu.a ^ val) & (g_cpu.a ^ diff) & 0x80) g_cpu.p |= FLAG_V;                 g_cpu.a = (uint8_t)diff;             }             cpu_set_nz(g_cpu.a);         }
        case 0xE9: case 0xEB: DO_SBC(mem_read(g_cpu.pc++)); break;
        case 0xE5: DO_SBC(mem_read(mem_read(g_cpu.pc++))); break;
        case 0xF5: DO_SBC(mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0xED: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_SBC(mem_read(a)); break; }
        case 0xFD: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_SBC(mem_read(a + g_cpu.x)); break; }
        case 0xF9: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_SBC(mem_read(a + g_cpu.y)); break; }
        case 0xE1: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_SBC(mem_read(a)); break; }
        case 0xF1: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_SBC(mem_read(a)); break; }

        #define DO_CMP(reg, expr) {             uint8_t val = (expr);             uint16_t r = (uint16_t)reg - val;             g_cpu.p &= ~(FLAG_C | FLAG_Z | FLAG_N);             if (reg >= val) g_cpu.p |= FLAG_C;             if (reg == val) g_cpu.p |= FLAG_Z;             if (r & 0x80) g_cpu.p |= FLAG_N;         }
        case 0xC9: DO_CMP(g_cpu.a, mem_read(g_cpu.pc++)); break;
        case 0xC5: DO_CMP(g_cpu.a, mem_read(mem_read(g_cpu.pc++))); break;
        case 0xD5: DO_CMP(g_cpu.a, mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0xCD: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_CMP(g_cpu.a, mem_read(a)); break; }
        case 0xDD: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_CMP(g_cpu.a, mem_read(a + g_cpu.x)); break; }
        case 0xD9: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_CMP(g_cpu.a, mem_read(a + g_cpu.y)); break; }
        case 0xC1: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_CMP(g_cpu.a, mem_read(a)); break; }
        case 0xD1: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_CMP(g_cpu.a, mem_read(a)); break; }

        case 0xE0: DO_CMP(g_cpu.x, mem_read(g_cpu.pc++)); break;
        case 0xE4: DO_CMP(g_cpu.x, mem_read(mem_read(g_cpu.pc++))); break;
        case 0xEC: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_CMP(g_cpu.x, mem_read(a)); break; }

        case 0xC0: DO_CMP(g_cpu.y, mem_read(g_cpu.pc++)); break;
        case 0xC4: DO_CMP(g_cpu.y, mem_read(mem_read(g_cpu.pc++))); break;
        case 0xCC: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_CMP(g_cpu.y, mem_read(a)); break; }

        #define DO_AND(expr) { uint8_t val = (expr); g_cpu.a &= val; cpu_set_nz(g_cpu.a); }
        case 0x29: DO_AND(mem_read(g_cpu.pc++)); break;
        case 0x25: DO_AND(mem_read(mem_read(g_cpu.pc++))); break;
        case 0x35: DO_AND(mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0x2D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_AND(mem_read(a)); break; }
        case 0x3D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_AND(mem_read(a + g_cpu.x)); break; }
        case 0x39: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_AND(mem_read(a + g_cpu.y)); break; }
        case 0x21: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_AND(mem_read(a)); break; }
        case 0x31: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_AND(mem_read(a)); break; }

        #define DO_ORA(expr) { uint8_t val = (expr); g_cpu.a |= val; cpu_set_nz(g_cpu.a); }
        case 0x09: DO_ORA(mem_read(g_cpu.pc++)); break;
        case 0x05: DO_ORA(mem_read(mem_read(g_cpu.pc++))); break;
        case 0x15: DO_ORA(mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0x0D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ORA(mem_read(a)); break; }
        case 0x1D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ORA(mem_read(a + g_cpu.x)); break; }
        case 0x19: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ORA(mem_read(a + g_cpu.y)); break; }
        case 0x01: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_ORA(mem_read(a)); break; }
        case 0x11: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_ORA(mem_read(a)); break; }

        #define DO_EOR(expr) { uint8_t val = (expr); g_cpu.a ^= val; cpu_set_nz(g_cpu.a); }
        case 0x49: DO_EOR(mem_read(g_cpu.pc++)); break;
        case 0x45: DO_EOR(mem_read(mem_read(g_cpu.pc++))); break;
        case 0x55: DO_EOR(mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0x4D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_EOR(mem_read(a)); break; }
        case 0x5D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_EOR(mem_read(a + g_cpu.x)); break; }
        case 0x59: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_EOR(mem_read(a + g_cpu.y)); break; }
        case 0x41: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_EOR(mem_read(a)); break; }
        case 0x51: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_EOR(mem_read(a)); break; }

        case 0x24: {
            uint8_t v = mem_read(mem_read(g_cpu.pc++));
            g_cpu.p &= ~(FLAG_N | FLAG_V | FLAG_Z);
            if (!(g_cpu.a & v)) g_cpu.p |= FLAG_Z;
            g_cpu.p |= (v & (FLAG_N | FLAG_V));
            break;
        }
        case 0x2C: {
            uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8);
            uint8_t v = mem_read(a);
            g_cpu.p &= ~(FLAG_N | FLAG_V | FLAG_Z);
            if (!(g_cpu.a & v)) g_cpu.p |= FLAG_Z;
            g_cpu.p |= (v & (FLAG_N | FLAG_V));
            break;
        }

        case 0x0A: { g_cpu.p &= ~FLAG_C; if (g_cpu.a & 0x80) g_cpu.p |= FLAG_C; g_cpu.a <<= 1; cpu_set_nz(g_cpu.a); break; }
        case 0x06: { uint8_t a = mem_read(g_cpu.pc++); uint8_t v = mem_read(a); g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v <<= 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x16: { uint8_t a = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint8_t v = mem_read(a); g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v <<= 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x0E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a); g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v <<= 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x1E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a + g_cpu.x); g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v <<= 1; mem_write(a + g_cpu.x, v); cpu_set_nz(v); break; }

        case 0x4A: { g_cpu.p &= ~FLAG_C; if (g_cpu.a & 0x01) g_cpu.p |= FLAG_C; g_cpu.a >>= 1; cpu_set_nz(g_cpu.a); break; }
        case 0x46: { uint8_t a = mem_read(g_cpu.pc++); uint8_t v = mem_read(a); g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v >>= 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x56: { uint8_t a = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint8_t v = mem_read(a); g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v >>= 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x4E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a); g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v >>= 1; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x5E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a + g_cpu.x); g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v >>= 1; mem_write(a + g_cpu.x, v); cpu_set_nz(v); break; }

        case 0x2A: { uint8_t c = (g_cpu.p & FLAG_C) ? 1 : 0; g_cpu.p &= ~FLAG_C; if (g_cpu.a & 0x80) g_cpu.p |= FLAG_C; g_cpu.a = ((g_cpu.a << 1) | c) & 0xFF; cpu_set_nz(g_cpu.a); break; }
        case 0x26: { uint8_t a = mem_read(g_cpu.pc++); uint8_t v = mem_read(a); uint8_t c = (g_cpu.p & FLAG_C) ? 1 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v = ((v << 1) | c) & 0xFF; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x36: { uint8_t a = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint8_t v = mem_read(a); uint8_t c = (g_cpu.p & FLAG_C) ? 1 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v = ((v << 1) | c) & 0xFF; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x2E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a); uint8_t c = (g_cpu.p & FLAG_C) ? 1 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v = ((v << 1) | c) & 0xFF; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x3E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a + g_cpu.x); uint8_t c = (g_cpu.p & FLAG_C) ? 1 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x80) g_cpu.p |= FLAG_C; v = ((v << 1) | c) & 0xFF; mem_write(a + g_cpu.x, v); cpu_set_nz(v); break; }

        case 0x6A: { uint8_t c = (g_cpu.p & FLAG_C) ? 0x80 : 0; g_cpu.p &= ~FLAG_C; if (g_cpu.a & 0x01) g_cpu.p |= FLAG_C; g_cpu.a = (g_cpu.a >> 1) | c; cpu_set_nz(g_cpu.a); break; }
        case 0x66: { uint8_t a = mem_read(g_cpu.pc++); uint8_t v = mem_read(a); uint8_t c = (g_cpu.p & FLAG_C) ? 0x80 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v = (v >> 1) | c; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x76: { uint8_t a = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint8_t v = mem_read(a); uint8_t c = (g_cpu.p & FLAG_C) ? 0x80 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v = (v >> 1) | c; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x6E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a); uint8_t c = (g_cpu.p & FLAG_C) ? 0x80 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v = (v >> 1) | c; mem_write(a, v); cpu_set_nz(v); break; }
        case 0x7E: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); uint8_t v = mem_read(a + g_cpu.x); uint8_t c = (g_cpu.p & FLAG_C) ? 0x80 : 0; g_cpu.p &= ~FLAG_C; if (v & 0x01) g_cpu.p |= FLAG_C; v = (v >> 1) | c; mem_write(a + g_cpu.x, v); cpu_set_nz(v); break; }

        default:
            break;
    }
}

/* ============================================================================
 * VIA 6522
 * ============================================================================ */
static void via_reset(void) {
    memset(&g_via, 0, sizeof(g_via));
    g_via.t1l = 0x2710;
    g_via.t1c = g_via.t1l;
    g_via.ier = 0x40;
    g_via.pcr = 0xCC;
    g_via.t1_active = true;
}

static void via_step(uint32_t cycles) {
    if (g_via.t1_active) {
        if (g_via.t1c <= cycles) {
            g_via.ifr |= 0x40;
            g_via.t1c = g_via.t1l - (cycles - g_via.t1c);
        } else {
            g_via.t1c -= cycles;
        }
    }

    if (g_via.t2_active) {
        if (g_via.t2c <= cycles) {
            g_via.ifr |= 0x20;
            g_via.t2c = 0xFFFF;
        } else {
            g_via.t2c -= cycles;
        }
    }

    g_cpu.irq_pending = (g_via.ifr & g_via.ier & 0x7F) != 0;
}

/* ============================================================================
 * AY-3-8912 (SON ET I/O CLAVIER)
 * ============================================================================ */
static void ay_reset(void) {
    memset(&g_ay.regs, 0, sizeof(g_ay.regs));
    g_ay.selected_reg = 0;
    g_ay.noise_lfsr = 1;
}

static void ay_write_reg(uint8_t reg, uint8_t val) {
    if (reg >= 16) return;
    g_ay.regs[reg] = val;

    g_ay.tone_period[0] = (g_ay.regs[0] | ((uint32_t)(g_ay.regs[1] & 0x0F) << 8));
    g_ay.tone_period[1] = (g_ay.regs[2] | ((uint32_t)(g_ay.regs[3] & 0x0F) << 8));
    g_ay.tone_period[2] = (g_ay.regs[4] | ((uint32_t)(g_ay.regs[5] & 0x0F) << 8));
    g_ay.noise_period   = (g_ay.regs[6] & 0x1F) * 2;
    g_ay.env_period     = (g_ay.regs[11] | ((uint32_t)g_ay.regs[12] << 8));
}

/* ============================================================================
 * VIDÉO ULA ORIC ATMOS (TEXT & HIRES)
 * ============================================================================ */
static void video_render_frame(void) {
    uint8_t mode_byte = mem_read(0xBFDF);
    if (mode_byte == 28 || mode_byte == 30) {
        if (!g_hires_mode) {
            g_hires_mode = true;
            debug_log(L"Bascule vidéo vers mode HIRES (240x200 + 3 lignes texte)");
        }
    } else if (mode_byte == 24 || mode_byte == 26) {
        if (g_hires_mode) {
            g_hires_mode = false;
            debug_log(L"Bascule vidéo vers mode TEXT (40x28)");
        }
    }
    if (g_ram[0x0213] != 0) {
        g_hires_mode = true;
    }

    bool blink_phase = (g_frame_counter / 16) & 1;

    for (int y = 0; y < 224; y++) {
        uint32_t ink = PALETTE[7];
        uint32_t paper = PALETTE[0];
        bool double_height = false;
        bool blinking = false;
        bool alt_charset = false;

        int row = y / 8;
        int char_line = y % 8;
        bool in_hires_area = g_hires_mode && (y < 200);

        if (in_hires_area) {
            uint16_t line_addr = 0xA000 + (y * 40);
            for (int col = 0; col < 40; col++) {
                uint8_t byte = g_ram[line_addr + col];
                if ((byte & 0x60) == 0) {
                    if (byte < 8) ink = PALETTE[byte & 7];
                    else if (byte >= 16 && byte < 24) paper = PALETTE[byte & 7];
                    for (int p = 0; p < 6; p++) {
                        g_framebuffer[y * SCREEN_W + (col * 6 + p)] = paper;
                    }
                } else {
                    for (int p = 0; p < 6; p++) {
                        bool pixel_on = (byte & (1 << (5 - p))) != 0;
                        g_framebuffer[y * SCREEN_W + (col * 6 + p)] = pixel_on ? ink : paper;
                    }
                }
            }
        } else {
            int text_row = g_hires_mode ? 25 + ((y - 200) / 8) : row;
            uint16_t row_addr = 0xBB80 + (text_row * 40);

            for (int col = 0; col < 40; col++) {
                uint8_t ch = g_ram[row_addr + col];

                if ((ch & 0x60) == 0) {
                    /* Octets d'attributs de série Oric */
                    if (ch < 8) {
                        ink = PALETTE[ch & 7];
                    } else if (ch >= 8 && ch < 16) {
                        alt_charset = (ch & 1) != 0;
                        double_height = (ch & 2) != 0;
                        blinking = (ch & 4) != 0;
                    } else if (ch >= 16 && ch < 24) {
                        paper = PALETTE[ch & 7];
                    }
                    for (int p = 0; p < 6; p++) {
                        g_framebuffer[y * SCREEN_W + (col * 6 + p)] = paper;
                    }
                } else {
                    bool inverse = (ch & 0x80) != 0;
                    uint8_t char_code = ch & 0x7F;

                    uint16_t font_base = alt_charset ? 0xB800 : 0xB400;
                    int glyph_line = double_height ? (char_line / 2) : char_line;
                    uint16_t glyph_addr = font_base + (char_code * 8) + glyph_line;

                    uint8_t pattern = g_ram[glyph_addr];

                    /* Recherche dans la table de polices standard intégrée si la RAM n'est pas encore initialisée */
                    if (pattern == 0 && char_code >= 32) {
                        pattern = ORIC_FONT_STANDARD[(char_code - 32) * 8 + glyph_line];

                        /* Remplacement par les caractères accentués français selon ISO-646-FR / Oric Atmos FR */
                        if (char_code == '@') pattern = ORIC_FRENCH_GLYPHS[0][glyph_line]; /* à */
                        else if (char_code == '{') pattern = ORIC_FRENCH_GLYPHS[1][glyph_line]; /* é */
                        else if (char_code == '}') pattern = ORIC_FRENCH_GLYPHS[2][glyph_line]; /* è */
                        else if (char_code == '~') pattern = ORIC_FRENCH_GLYPHS[3][glyph_line]; /* ê */
                        else if (char_code == '\\') pattern = ORIC_FRENCH_GLYPHS[4][glyph_line]; /* ç */
                        else if (char_code == '|') pattern = ORIC_FRENCH_GLYPHS[5][glyph_line]; /* ù */
                        else if (char_code == '[') pattern = ORIC_FRENCH_GLYPHS[6][glyph_line]; /* ô */
                        else if (char_code == ']') pattern = ORIC_FRENCH_GLYPHS[7][glyph_line]; /* î */
                    }

                    if (blinking && blink_phase) {
                        pattern = 0;
                    }

                    for (int p = 0; p < 6; p++) {
                        bool dot = (pattern & (1 << (5 - p))) != 0;
                        if (inverse) dot = !dot;
                        g_framebuffer[y * SCREEN_W + (col * 6 + p)] = dot ? ink : paper;
                    }
                }
            }
        }
    }

    g_frame_counter++;
}

/* ============================================================================
 * CHARGEMENT ROM
 * ============================================================================ */
static void oric_load_rom(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return;

    size_t n = fread(g_rom, 1, 16384, f);
    fclose(f);

    if (n == 16384) {
        g_rom_loaded = true;
        debug_log(L"ROM chargée avec succès : %ls (16384 octets)", path);
    } else {
        debug_log(L"Erreur : Taille de ROM incorrecte (%u octets) pour %ls", (unsigned)n, path);
    }
}

/* ============================================================================
 * CHARGEMENT .TAP ET AUTO-RUN (TURBO TAPE)
 * ============================================================================ */
static void oric_inject_tap_and_run(const wchar_t *filename) {
    FILE *f = _wfopen(filename, L"rb");
    if (!f) {
        wchar_t msg[512];
        swprintf(msg, 512, L"Impossible d'ouvrir le fichier .TAP :\n%ls", filename);
        MessageBoxW(NULL, msg, L"Erreur fichier", MB_OK | MB_ICONERROR);
        return;
    }

    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)malloc(fsize);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, fsize, f);
    fclose(f);

    debug_log(L"Fichier .TAP ouvert : %ls (Taille: %u octets)", filename, (unsigned)fsize);

    if (g_tape.data) free(g_tape.data);
    g_tape.data = buf;
    g_tape.size = fsize;
    g_tape.pos = 0;
    g_tape.active = true;

    size_t ptr = 0;
    int blocks_loaded = 0;
    uint16_t first_start = 0;
    uint8_t first_type = 0;
    

    while (ptr < fsize) {
        while (ptr < fsize && buf[ptr] != 0x24) ptr++;
        if (ptr >= fsize) break;
        ptr++;

        if (ptr + 9 > fsize) break;

        uint8_t file_type = buf[ptr + 2];
        uint8_t autorun   = buf[ptr + 3];
        uint16_t end_addr   = ((uint16_t)buf[ptr + 4] << 8) | buf[ptr + 5];
        uint16_t start_addr = ((uint16_t)buf[ptr + 6] << 8) | buf[ptr + 7];
        ptr += 9;

        char block_name[32] = {0};
        int ni = 0;
        while (ptr < fsize && buf[ptr] != 0 && ni < 31) {
            block_name[ni++] = (char)buf[ptr++];
        }
        if (ptr < fsize && buf[ptr] == 0) ptr++;

        if (blocks_loaded == 0) {
            memcpy(g_tape.current_name, block_name, sizeof(g_tape.current_name) - 1);
            g_tape.current_name[sizeof(g_tape.current_name) - 1] = 0;
            first_start = start_addr;
            first_type = file_type;
            (void)autorun;
        }

        size_t data_len = 0;
        if (end_addr >= start_addr) {
            data_len = end_addr - start_addr + 1;
        }

        debug_log(L"Bloc TAP #%d : Nom=\"%hs\", Type=0x%02X (%ls), Début=0x%04X, Fin=0x%04X, Longueur=%u, Autorun=0x%02X",
                  blocks_loaded + 1, block_name, file_type,
                  (file_type & 0x80) ? L"Code Machine" : L"BASIC",
                  start_addr, end_addr, (unsigned)data_len, autorun);

        if (ptr + data_len <= fsize && data_len > 0) {
            for (size_t i = 0; i < data_len; i++) {
                g_ram[start_addr + (uint16_t)i] = buf[ptr + i];
            }
            ptr += data_len;

            if ((file_type & 0x80) == 0) {
                uint16_t end_p = end_addr + 1;
                g_ram[0x9A] = end_p & 0xFF;
                g_ram[0x9B] = (end_p >> 8) & 0xFF;
                g_ram[0x9C] = end_p & 0xFF;
                g_ram[0x9D] = (end_p >> 8) & 0xFF;
            }
            blocks_loaded++;
        } else {
            break;
        }
    }

    if (blocks_loaded > 0) {
        if (first_type & 0x80) {
            debug_log(L"Démarrage automatique Code Machine à l'adresse 0x%04X", first_start);
            g_cpu.pc = first_start;
        } else {
            debug_log(L"Démarrage automatique programme BASIC");
            g_ram[0x02DF] = 'R';
            g_ram[0x0208] = 0;
        }
    }
}

/* ============================================================================
 * CLAVIER & MAPPING AZERTY / QWERTY
 * ============================================================================ */
static void keyboard_set_key(int row, int col, bool pressed) {
    if (row < 0 || row > 7 || col < 0 || col > 7) return;
    if (pressed) {
        g_key_matrix[row] |= (1 << col);
    } else {
        g_key_matrix[row] &= ~(1 << col);
    }
}

static void keyboard_handle_vk(UINT vk, bool pressed) {
    const wchar_t *key_name = L"";
    int row = -1, col = -1;

    switch (vk) {
        /* --- Touches de contrôle et navigation --- */
        case VK_SPACE:   row = 4; col = 0; key_name = L"ESPACE"; break;
        case VK_RETURN:  row = 7; col = 5; key_name = L"ENTRÉE"; break;
        case VK_ESCAPE:  row = 1; col = 5; key_name = L"ÉCHAP"; break;
        case VK_BACK:    row = 5; col = 5; key_name = L"RETOUR"; break;
        case VK_DELETE:  row = 5; col = 5; key_name = L"SUPPR"; break;
        case VK_TAB:     row = 4; col = 7; key_name = L"TAB"; break;

        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:  row = 4; col = 4; key_name = L"MAJ"; break;

        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL: row = 2; col = 4; key_name = L"CTRL"; break;

        /* --- Flèches directionnelles --- */
        case VK_LEFT:    row = 4; col = 5; key_name = L"GAUCHE"; break;
        case VK_RIGHT:   row = 4; col = 7; key_name = L"DROITE"; break;
        case VK_DOWN:    row = 4; col = 6; key_name = L"BAS"; break;
        case VK_UP:      row = 4; col = 3; key_name = L"HAUT"; break;

        /* --- Chiffres 0-9 (principal et pavé numérique) --- */
        case '0':
        case VK_NUMPAD0: row = 7; col = 2; key_name = L"0"; break;
        case '1':
        case VK_NUMPAD1: row = 0; col = 5; key_name = L"1"; break;
        case '2':
        case VK_NUMPAD2: row = 2; col = 6; key_name = L"2"; break;
        case '3':
        case VK_NUMPAD3: row = 0; col = 7; key_name = L"3"; break;
        case '4':
        case VK_NUMPAD4: row = 2; col = 3; key_name = L"4"; break;
        case '5':
        case VK_NUMPAD5: row = 0; col = 2; key_name = L"5"; break;
        case '6':
        case VK_NUMPAD6: row = 2; col = 1; key_name = L"6"; break;
        case '7':
        case VK_NUMPAD7: row = 0; col = 0; key_name = L"7"; break;
        case '8':
        case VK_NUMPAD8: row = 7; col = 0; key_name = L"8"; break;
        case '9':
        case VK_NUMPAD9: row = 3; col = 1; key_name = L"9"; break;

        /* --- Lettres A-Z --- */
        case 'A': row = 6; col = 5; key_name = L"A"; break;
        case 'B': row = 2; col = 2; key_name = L"B"; break;
        case 'C': row = 2; col = 7; key_name = L"C"; break;
        case 'D': row = 1; col = 7; key_name = L"D"; break;
        case 'E': row = 6; col = 3; key_name = L"E"; break;
        case 'F': row = 1; col = 3; key_name = L"F"; break;
        case 'G': row = 6; col = 2; key_name = L"G"; break;
        case 'H': row = 6; col = 1; key_name = L"H"; break;
        case 'I': row = 5; col = 1; key_name = L"I"; break;
        case 'J': row = 1; col = 0; key_name = L"J"; break;
        case 'K': row = 3; col = 0; key_name = L"K"; break;
        case 'L': row = 7; col = 1; key_name = L"L"; break;
        case 'M': row = 2; col = 0; key_name = L"M"; break;
        case 'N': row = 0; col = 1; key_name = L"N"; break;
        case 'O': row = 5; col = 2; key_name = L"O"; break;
        case 'P': row = 5; col = 3; key_name = L"P"; break;
        case 'Q': row = 1; col = 6; key_name = L"Q"; break;
        case 'R': row = 1; col = 2; key_name = L"R"; break;
        case 'S': row = 6; col = 6; key_name = L"S"; break;
        case 'T': row = 1; col = 1; key_name = L"T"; break;
        case 'U': row = 5; col = 0; key_name = L"U"; break;
        case 'V': row = 0; col = 3; key_name = L"V"; break;
        case 'W': row = 6; col = 7; key_name = L"W"; break;
        case 'X': row = 0; col = 6; key_name = L"X"; break;
        case 'Y': row = 6; col = 0; key_name = L"Y"; break;
        case 'Z': row = 2; col = 5; key_name = L"Z"; break;

        /* --- Ponctuation & Symboles --- */
        case VK_OEM_COMMA:  row = 4; col = 1; key_name = L","; break;
        case VK_OEM_PERIOD: row = 4; col = 2; key_name = L"."; break;
        case VK_DECIMAL:    row = 4; col = 2; key_name = L"."; break;
        case VK_OEM_1:      row = 3; col = 2; key_name = L";"; break;
        case VK_OEM_MINUS:  row = 3; col = 3; key_name = L"-"; break;
        case VK_SUBTRACT:   row = 3; col = 3; key_name = L"-"; break;
        case VK_OEM_2:      row = 7; col = 3; key_name = L"/"; break;
        case VK_DIVIDE:     row = 7; col = 3; key_name = L"/"; break;
        case VK_OEM_PLUS:   row = 7; col = 7; key_name = L"="; break;
        case VK_ADD:        row = 7; col = 7; key_name = L"+"; break;
        case VK_MULTIPLY:   row = 7; col = 0; key_name = L"*"; break;
        case VK_OEM_4:      row = 5; col = 7; key_name = L"["; break;
        case VK_OEM_6:      row = 5; col = 6; key_name = L"]"; break;
        case VK_OEM_5:      row = 3; col = 6; key_name = L"\\"; break;
        case VK_OEM_7:      row = 3; col = 7; key_name = L"'"; break;
        case VK_OEM_8:      row = 0; col = 5; key_name = L"!"; break;
    }

    if (row >= 0 && col >= 0) {
        keyboard_set_key(row, col, pressed);
        debug_log(L"Touche %ls : VK=0x%02X (%ls) -> Ligne %d, Col %d",
                  pressed ? L"ENFONCÉE" : L"RELÂCHÉE", vk, key_name, row, col);
    }
}

/* ============================================================================
 * AUDIO WIN32 (WAVEOUT)
 * ============================================================================ */
static void audio_init(HWND hwnd) {
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 22050;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = 22050 * 2;

    if (waveOutOpen(&g_ay.hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)hwnd, 0, CALLBACK_WINDOW) == MMSYSERR_NOERROR) {
        for (int i = 0; i < 2; i++) {
            memset(&g_ay.waveHdr[i], 0, sizeof(WAVEHDR));
            g_ay.waveHdr[i].lpData = (LPSTR)g_ay.audioBuffer[i];
            g_ay.waveHdr[i].dwBufferLength = sizeof(g_ay.audioBuffer[i]);
            waveOutPrepareHeader(g_ay.hWaveOut, &g_ay.waveHdr[i], sizeof(WAVEHDR));
            g_ay.waveHdr[i].dwFlags |= WHDR_DONE;
        }
        g_ay.audio_enabled = true;
        debug_log(L"Périphérique audio WaveOut initialisé (22050 Hz, 16 bits)");
    } else {
        g_ay.audio_enabled = false;
        debug_log(L"Avertissement : Impossible d'initialiser le périphérique audio WaveOut");
    }
}

static void audio_update(void) {
    if (!g_ay.audio_enabled) return;

    int buf = g_ay.currentBuffer;
    if (!(g_ay.waveHdr[buf].dwFlags & WHDR_DONE)) return;

    short *out = g_ay.audioBuffer[buf];
    int samples = sizeof(g_ay.audioBuffer[0]) / sizeof(short);

    for (int i = 0; i < samples; i++) {
        int sample = 0;
        for (int ch = 0; ch < 3; ch++) {
            uint32_t per = g_ay.tone_period[ch];
            if (per > 5) {
                g_ay.tone_counter[ch] += 22050;
                uint32_t freq = 1000000 / (16 * per);
                if (freq > 0 && ((i * freq / 22050) & 1)) {
                    uint8_t vol = g_ay.regs[8 + ch] & 0x0F;
                    sample += (vol * 350);
                }
            }
        }
        out[i] = (short)sample;
    }

    waveOutWrite(g_ay.hWaveOut, &g_ay.waveHdr[buf], sizeof(WAVEHDR));
    g_ay.currentBuffer = 1 - buf;
}

static void audio_shutdown(void) {
    if (g_ay.audio_enabled && g_ay.hWaveOut) {
        waveOutReset(g_ay.hWaveOut);
        for (int i = 0; i < 2; i++) {
            waveOutUnprepareHeader(g_ay.hWaveOut, &g_ay.waveHdr[i], sizeof(WAVEHDR));
        }
        waveOutClose(g_ay.hWaveOut);
        g_ay.hWaveOut = NULL;
    }
}

/* ============================================================================
 * FENÊTRE PRINCIPALE WIN32
 * ============================================================================ */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            DragAcceptFiles(hwnd, TRUE);
            memset(&g_bmi, 0, sizeof(g_bmi));
            g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            g_bmi.bmiHeader.biWidth = SCREEN_W;
            g_bmi.bmiHeader.biHeight = -SCREEN_H;
            g_bmi.bmiHeader.biPlanes = 1;
            g_bmi.bmiHeader.biBitCount = 32;
            g_bmi.bmiHeader.biCompression = BI_RGB;
            audio_init(hwnd);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            wchar_t filename[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, filename, MAX_PATH)) {
                wchar_t *ext = wcsrchr(filename, L'.');
                if (ext && (_wcsicmp(ext, L".tap") == 0)) {
                    debug_log(L"Fichier .TAP déposé par l'utilisateur : %ls", filename);
                    oric_inject_tap_and_run(filename);
                    wchar_t title[512];
                    swprintf(title, 512, L"Oric Atmos - [Jeu chargé : %hs] - Glisser un autre .TAP pour changer",
                             g_tape.current_name[0] ? g_tape.current_name : "Inconnu");
                    SetWindowTextW(hwnd, title);
                } else if (ext && (_wcsicmp(ext, L".rom") == 0)) {
                    debug_log(L"Fichier .ROM déposé par l'utilisateur : %ls", filename);
                    oric_load_rom(filename);
                    cpu_reset();
                    via_reset();
                    ay_reset();
                    SetWindowTextW(hwnd, L"Oric Atmos - [Nouvelle ROM chargée]");
                }
            }
            DragFinish(hDrop);
            return 0;
        }

        case WM_KEYDOWN:
            keyboard_handle_vk((UINT)wParam, true);
            if (wParam == VK_F5) {
                debug_log(L"Réinitialisation matérielle (Touche F5 enfoncée)");
                cpu_reset();
                via_reset();
                ay_reset();
            } else if (wParam == VK_F1) {
                MessageBoxW(hwnd,
                    L"Émulateur Oric Atmos Win32 pur (MinGW64 / C Unicode)\n\n"
                    L"Commandes :\n"
                    L"- Glisser-déposer un fichier .TAP sur la fenêtre pour lancer un jeu\n"
                    L"- Glisser-déposer un fichier .ROM pour charger une autre ROM\n"
                    L"- F5 : Réinitialiser la machine (Reset matériel 6502)\n"
                    L"- Clavier : Support AZERTY / QWERTY complet\n",
                    L"Aide Oric Atmos", MB_OK | MB_ICONINFORMATION);
            }
            return 0;

        case WM_KEYUP:
            keyboard_handle_vk((UINT)wParam, false);
            return 0;

        case WM_KILLFOCUS:
            memset(g_key_matrix, 0, sizeof(g_key_matrix));
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
 * INITIALISATION PRINCIPALE ET POINT D'ENTRÉE UNICODE (wWinMain / wmain)
 * ============================================================================ */
static int run_emulator(HINSTANCE hInstance, const wchar_t *cmdLine, int nCmdShow) {
    debug_init();

    oric_load_rom(L"110fr.rom");
    if (!g_rom_loaded) oric_load_rom(L"atmos.rom");
    if (!g_rom_loaded) oric_load_rom(L"stock_basic11b.rom");

    if (!g_rom_loaded) {
        debug_log(L"Avertissement : Fichier ROM introuvable dans le répertoire courant");
        MessageBoxW(NULL,
            L"Impossible de trouver '110fr.rom' dans le dossier courant.\n"
            L"Veuillez vous assurer que '110fr.rom' est situé à côté de l'exécutable.",
            L"ROM non trouvée", MB_OK | MB_ICONWARNING);
    }

    memset(g_ram, 0, sizeof(g_ram));
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
        debug_log(L"Erreur : Impossible d'enregistrer la classe WNDCLASSW");
        return 1;
    }

    RECT rc = {0, 0, WIN_W, WIN_H};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"OricAtmosWin32",
        L"Oric Atmos (MinGW64 / C Unicode / GDI) - Glissez un fichier .TAP sur la fenêtre",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        debug_log(L"Erreur : Impossible de créer la fenêtre principale Win32");
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    debug_log(L"Fenêtre d'émulation ouverte (%dx%d)", rc.right - rc.left, rc.bottom - rc.top);

    if (cmdLine && cmdLine[0] != 0) {
        wchar_t path[MAX_PATH];
        wcsncpy(path, cmdLine, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
        if (path[0] == L'\"') {
            size_t l = wcslen(path);
            if (path[l - 1] == L'\"') path[l - 1] = 0;
            wmemmove(path, path + 1, l);
        }
        oric_inject_tap_and_run(path);
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

        uint64_t target_cycles = g_cpu.cycles + CYCLES_PER_FRAME;
        while (g_cpu.cycles < target_cycles) {
            uint64_t before = g_cpu.cycles;
            cpu_step();
            uint32_t elapsed = (uint32_t)(g_cpu.cycles - before);
            via_step(elapsed);
        }

        video_render_frame();
        audio_update();
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
    return run_emulator(hInstance, lpCmdLine, nCmdShow);
}

int wmain(int argc, wchar_t *argv[]) {
    HINSTANCE hInst = GetModuleHandleW(NULL);
    const wchar_t *arg = (argc > 1) ? argv[1] : L"";
    return run_emulator(hInst, arg, SW_SHOWNORMAL);
}
