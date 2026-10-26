#include "keyboard.h"

uint8_t g_key_matrix[8] = {0};

/* --- Buffer de saisie automatique (Auto-CLOAD & commandes) --- */
static char g_typein_buffer[128] = {0};
static int g_typein_pos = 0;
static int g_typein_len = 0;
static uint32_t g_typein_timer = 0;
static int g_typein_row = -1;
static int g_typein_col = -1;
static bool g_typein_shift = false;

void keyboard_reset(void) {
    memset(g_key_matrix, 0, sizeof(g_key_matrix));
    g_typein_pos = 0;
    g_typein_len = 0;
    g_typein_timer = 0;
    g_typein_row = -1;
    g_typein_col = -1;
    g_typein_shift = false;
    memset(g_typein_buffer, 0, sizeof(g_typein_buffer));
}

void setBit(int row, int bit, UINT pressed)
{
    if (pressed)
        g_key_matrix[row] |= (unsigned char)(1u << bit);
    else
        g_key_matrix[row] &= (unsigned char)~(1u << bit);
}

void keyboard_set_key(int row, int col, bool pressed) {

    if (row < 0 || row > 7 || col < 0 || col > 7) return;

    UINT lshift = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0; // 4 4
    UINT rshift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0; // 7 4
    UINT lctrl  = (GetAsyncKeyState(VK_LCONTROL) & 0x8000)!=0; // 2 4
    UINT rctrl  = (GetAsyncKeyState(VK_RCONTROL) & 0x8000)!=0; // 5 4

    setBit(4,4,lshift);
    setBit(7,4,rshift);
    setBit(2,4,lctrl);
    setBit(5,4,rctrl);

    if (pressed) {
        g_key_matrix[row] |= (1 << col);
    } else {
        g_key_matrix[row] &= ~(1 << col);
    }
}

uint8_t keyboard_read_bit(uint8_t row, uint8_t col_mask) {
    uint8_t r = row & 0x07;
    return (g_key_matrix[r] & col_mask) ? 0x08 : 0x00;
}

/* ============================================================================
 * CONVERSION CARACTÈRE ASCII -> MATRICE CLAVIER ORIC ATMOS
 * Table issue directement du décodage ROM Atmos ($FF78)
 * ============================================================================ */
bool keyboard_char_to_matrix(char ch, int *out_row, int *out_col, bool *out_shift) {

    if (true) return false; //@todo

    printf("keyboard_char_to_matrix() %d\n",ch);

    if (!out_row || !out_col || !out_shift) return false;
    *out_shift = false;

    switch (ch) {
        case '\x08':
        case '\x7f': *out_row = 5; *out_col = 5; *out_shift = false; return true; /* Oric Atmos DEL: efface le caractère */
        case '\t':   *out_row = 4; *out_col = 7; *out_shift = false; return true; /* Tab */
        case '\r':
        case '\n':   *out_row = 7; *out_col = 5; *out_shift = false; return true; /* Return */
        case '\x1b': *out_row = 1; *out_col = 5; *out_shift = false; return true; /* Escape */
        case ' ':    *out_row = 4; *out_col = 0; *out_shift = false; return true; /* Espace */

        /* Symboles décalés (Shift requis sur Atmos) */
        case '!': *out_row = 0; *out_col = 5; *out_shift = true; return true;
        case '"': *out_row = 3; *out_col = 7; *out_shift = true; return true; /* Shift + ' donne " */
        // case '#': *out_row = 0; *out_col = 7; *out_shift = true; return true; /* Shift + 3 donne # */
        case '$': *out_row = 2; *out_col = 3; *out_shift = true; return true;
        case '%': *out_row = 0; *out_col = 2; *out_shift = true; return true;
        case '&': *out_row = 0; *out_col = 0; *out_shift = true; return true;
        case '\'': *out_row = 3; *out_col = 7; *out_shift = false; return true;
        case '(': *out_row = 3; *out_col = 1; *out_shift = true; return true;
        case ')': *out_row = 7; *out_col = 2; *out_shift = true; return true;
        case '*': *out_row = 7; *out_col = 0; *out_shift = true; return true;
        case '+': *out_row = 7; *out_col = 7; *out_shift = true; return true;
        case ',': *out_row = 4; *out_col = 1; *out_shift = false; return true;
        case '-': *out_row = 3; *out_col = 3; *out_shift = false; return true;
        case '.': *out_row = 4; *out_col = 2; *out_shift = false; return true;
        case '/': *out_row = 7; *out_col = 3; *out_shift = false; return true;

        /* Chiffres 0 à 9 */
        case '0': *out_row = 7; *out_col = 2; *out_shift = false; return true;
        case '1': *out_row = 0; *out_col = 5; *out_shift = false; return true;
        case '2': *out_row = 2; *out_col = 6; *out_shift = false; return true;
        case '3': *out_row = 0; *out_col = 7; *out_shift = false; return true;
        case '4': *out_row = 2; *out_col = 3; *out_shift = false; return true;
        case '5': *out_row = 0; *out_col = 2; *out_shift = false; return true;
        case '6': *out_row = 2; *out_col = 1; *out_shift = false; return true;
        case '7': *out_row = 0; *out_col = 0; *out_shift = false; return true;
        case '8': *out_row = 7; *out_col = 0; *out_shift = false; return true;
        case '9': *out_row = 3; *out_col = 1; *out_shift = false; return true;

        case ':': *out_row = 3; *out_col = 2; *out_shift = true; return true;
        case ';': *out_row = 3; *out_col = 2; *out_shift = false; return true;
        case '<': *out_row = 4; *out_col = 1; *out_shift = true; return true;
        case '=': *out_row = 7; *out_col = 7; *out_shift = false; return true;
        case '>': *out_row = 4; *out_col = 2; *out_shift = true; return true;
        case '?': *out_row = 7; *out_col = 3; *out_shift = true; return true;
        case '@': *out_row = 2; *out_col = 6; *out_shift = true; return true;

        /* Lettres A à Z (insensibles à la casse pour les commandes BASIC) */
        case 'A': case 'a': *out_row = 6; *out_col = 5; return true;
        case 'B': case 'b': *out_row = 2; *out_col = 2; return true;
        case 'C': case 'c': *out_row = 2; *out_col = 7; return true;
        case 'D': case 'd': *out_row = 1; *out_col = 7; return true;
        case 'E': case 'e': *out_row = 6; *out_col = 3; return true;
        case 'F': case 'f': *out_row = 1; *out_col = 3; return true;
        case 'G': case 'g': *out_row = 6; *out_col = 2; return true;
        case 'H': case 'h': *out_row = 6; *out_col = 1; return true;
        case 'I': case 'i': *out_row = 5; *out_col = 1; return true;
        case 'J': case 'j': *out_row = 1; *out_col = 0; return true;
        case 'K': case 'k': *out_row = 3; *out_col = 0; return true;
        case 'L': case 'l': *out_row = 7; *out_col = 1; return true;
        case 'M': case 'm': *out_row = 2; *out_col = 0; return true;
        case 'N': case 'n': *out_row = 0; *out_col = 1; return true;
        case 'O': case 'o': *out_row = 5; *out_col = 2; return true;
        case 'P': case 'p': *out_row = 5; *out_col = 3; return true;
        case 'Q': case 'q': *out_row = 1; *out_col = 6; return true;
        case 'R': case 'r': *out_row = 1; *out_col = 2; return true;
        case 'S': case 's': *out_row = 6; *out_col = 6; return true;
        case 'T': case 't': *out_row = 1; *out_col = 1; return true;
        case 'U': case 'u': *out_row = 5; *out_col = 0; return true;
        case 'V': case 'v': *out_row = 0; *out_col = 3; return true;
        case 'W': case 'w': *out_row = 6; *out_col = 7; return true;
        case 'X': case 'x': *out_row = 0; *out_col = 6; return true;
        case 'Y': case 'y': *out_row = 6; *out_col = 0; return true;
        case 'Z': case 'z': *out_row = 2; *out_col = 5; return true;

        case '[': *out_row = 5; *out_col = 7; *out_shift = false; return true;
        case '\\': *out_row = 3; *out_col = 6; *out_shift = false; return true;
        case ']': *out_row = 5; *out_col = 6; *out_shift = false; return true;
        case '^': *out_row = 2; *out_col = 1; *out_shift = true; return true;
        case '_': *out_row = 3; *out_col = 3; *out_shift = true; return true;
        case '{': *out_row = 5; *out_col = 7; *out_shift = true; return true;
        case '|': *out_row = 3; *out_col = 6; *out_shift = true; return true;
        case '}': *out_row = 5; *out_col = 6; *out_shift = true; return true;
    }
    return false;
}

/* ============================================================================
 * GESTION DES TOUCHES PHYSIQUES WINDOWS (VK_*)
 * Support complet AZERTY et QWERTY
 * ============================================================================ */
void keyboard_handle_vk(UINT vk, DWORD scancode, bool pressed) {

    WCHAR key=(vk<32)?32:vk;

    scancode=scancode >> 16 & 0x3fff;

    debug_log(L"Clavier : VK=0x%02X SC=%02X %c", vk,scancode,key);

    int row = -1, col = -1;

    switch (scancode) {
        case 0x0b: case 0x52: row = 7; col = 2; break; // 0
        case 0x02: case 0x4f: row = 0; col = 5; break; // 1
        case 0x03: case 0x50: row = 2; col = 6; break; // 2
        case 0x04: case 0x51: row = 0; col = 7; break; // 3
        case 0x05: case 0x4b: row = 2; col = 3; break; // 4
        case 0x06: case 0x4c: row = 0; col = 2; break; // 5
        case 0x07: case 0x4d: row = 2; col = 1; break; // 6
        case 0x08: case 0x47: row = 0; col = 0; break; // 7
        case 0x09: case 0x48: row = 7; col = 0; break; // 8
        case 0x0a: case 0x49: row = 3; col = 1; break; // 9
        case 0x0c: row = 3; col = 3; break; // -£
        case 0x0d: row = 7; col = 7; break; // =+

        case 0x0E: row = 5; col = 5; break; // Backspace


/*
        case '3': {
            bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (shift_down) {
                row = 0; col = 7;
            } else {
                row = 3; col = 7;
                keyboard_set_key(4, 4, pressed);
            }
            break;
        }
        case VK_NUMPAD3: row = 0; col = 7; break;
*/


        case 0x10: row = 1; col = 6; break; // Q
        case 0x11: row = 6; col = 7; break; // W
        case 0x12: row = 6; col = 3; break; // E
        case 0x13: row = 1; col = 2; break; // R
        case 0x14: row = 1; col = 1; break; // T
        case 0x15: row = 6; col = 0; break; // Y
        case 0x16: row = 5; col = 0; break; // U
        case 0x17: row = 5; col = 1; break; // I
        case 0x18: row = 5; col = 2; break; // O
        case 0x19: row = 5; col = 3; break; // P
        case 0x1a: row = 5; col = 7; break; // [
        case 0x1b: row = 5; col = 6; break; // ]

        case 0x1c: case 0x11c: row = 7; col = 5; break; // Ret & Enter
        case 0x1e: row = 6; col = 5; break; // A
        case 0x1f: row = 6; col = 6; break; // S
        case 0x20: row = 1; col = 7; break; // D
        case 0x21: row = 1; col = 3; break; // F
        case 0x22: row = 6; col = 2; break; // G
        case 0x23: row = 6; col = 1; break; // H
        case 0x24: row = 1; col = 0; break; // J
        case 0x25: row = 3; col = 0; break; // K
        case 0x26: row = 7; col = 1; break; // L
        case 0x27: row = 3; col = 2; break; // ; :
        case 0x28: row = 3; col = 7; break; // ' "

        // case 0x29: row = 3; col = 7; break; // ' "
        // case 0x2a: row = 3; col = 7; break; // ' "


        case 0x2c: row = 2; col = 5; break; // Z
        case 0x2d: row = 0; col = 6; break; // X
        case 0x2e: row = 2; col = 7; break; // C
        case 0x2f: row = 0; col = 3; break; // V
        case 0x30: row = 2; col = 2; break; // B
        case 0x31: row = 0; col = 1; break; // N
        case 0x32: row = 2; col = 0; break; // M
        case 0x33: row = 4; col = 1; break; // ,<
        case 0x34: case 0x53: row = 4; col = 2; break; // .>
        case 0x35: row = 7; col = 3; break; // /?

        case 0x39: row = 4; col = 0; break; // Space

        case 0x148: row=4; col=3;break; // Up
        case 0x150: row=4; col=6;break; // Down
        case 0x14b: row=4; col=5;break; // Left
        case 0x14d: row=4; col=7;break; // Right

        // Azerty vs Qwerty

        case 0x2b: row = 3; col = 6; break; // azerty *µ qwerty |\

        case 0x56: row = 4; col = 0; break; // <>


    }

    if (row >= 0 && col >= 0) {
        keyboard_set_key(row, col, pressed);
        if (pressed) {
            debug_log(L"Clavier : VK=0x%02X -> Ligne %d, Colonne %d enfoncée", vk, row, col);
        }
    }
}

/* ============================================================================
 * SAISIE AUTOMATIQUE (AUTO-CLOAD & COMMANDES)
 * ============================================================================ */
void keyboard_queue_char(char ch) {
    if (g_typein_len >= 126) return;
    if (g_typein_pos >= g_typein_len) {
        g_typein_pos = 0;
        g_typein_len = 0;
    }
    g_typein_buffer[g_typein_len++] = ch;
    g_typein_buffer[g_typein_len] = 0;
    if (g_typein_row < 0 && g_typein_timer == 0) {
        g_typein_timer = 2000;
    }
}

void typein_queue(const char *str) {
    if (!str) return;
    int len = (int)strlen(str);
    if (len >= 120) len = 120;
    memcpy(g_typein_buffer, str, len);
    g_typein_buffer[len] = 0;
    g_typein_len = len;
    g_typein_pos = 0;
    g_typein_timer = 120000; /* Pause de 120ms avant la première touche */
    g_typein_row = -1;
    g_typein_col = -1;
    g_typein_shift = false;
}

void typein_step(uint32_t cycles) {
    if (g_typein_pos >= g_typein_len) return;

    if (g_typein_timer > cycles) {
        g_typein_timer -= cycles;
        return;
    }
    g_typein_timer = 0;

    /* Si une touche était enfoncée, on la relâche */
    if (g_typein_row >= 0) {
        keyboard_set_key(g_typein_row, g_typein_col, false);
        if (g_typein_shift) {
            keyboard_set_key(4, 4, false); /* Relâche Shift */
            g_typein_shift = false;
        }
        g_typein_row = -1;
        g_typein_col = -1;
        g_typein_timer = 50000; /* Pause de 50ms entre les touches */
        g_typein_pos++;
        return;
    }

    char ch = g_typein_buffer[g_typein_pos];
    int row = -1, col = -1;
    bool shift = false;

    if (keyboard_char_to_matrix(ch, &row, &col, &shift)) {
        g_typein_row = row;
        g_typein_col = col;
        g_typein_shift = shift;
        keyboard_set_key(row, col, true);
        if (shift) {
            keyboard_set_key(4, 4, true); /* Left Shift */
        }
        g_typein_timer = 65000; /* Touche maintenue 65ms pour détection fiable */
    } else {
        g_typein_pos++;
    }
}
