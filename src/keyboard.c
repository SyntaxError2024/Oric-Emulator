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

void keyboard_set_key(int row, int col, bool pressed) {
    if (row < 0 || row > 7 || col < 0 || col > 7) return;
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
    if (!out_row || !out_col || !out_shift) return false;
    *out_shift = false;

    switch (ch) {
        case '\x08': *out_row = 4; *out_col = 5; *out_shift = false; return true; /* Backspace / Gauche */
        case '\t':   *out_row = 4; *out_col = 7; *out_shift = false; return true; /* Tab */
        case '\r':
        case '\n':   *out_row = 7; *out_col = 5; *out_shift = false; return true; /* Return */
        case '\x1b': *out_row = 1; *out_col = 5; *out_shift = false; return true; /* Escape */
        case ' ':    *out_row = 4; *out_col = 0; *out_shift = false; return true; /* Espace */

        /* Symboles décalés (Shift requis sur Atmos) */
        case '!': *out_row = 0; *out_col = 5; *out_shift = true; return true;
        case '"': *out_row = 3; *out_col = 7; *out_shift = true; return true; /* Shift + ' donne " */
        case '#': *out_row = 0; *out_col = 7; *out_shift = true; return true; /* Shift + 3 donne # */
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
void keyboard_handle_vk(UINT vk, bool pressed) {
    int row = -1, col = -1;

    switch (vk) {
        case VK_SPACE:   row = 4; col = 0; break;
        case VK_RETURN:  row = 7; col = 5; break;
        case VK_ESCAPE:  row = 1; col = 5; break;
        case VK_BACK:    row = 5; col = 5; break;
        case VK_DELETE:  row = 5; col = 5; break;
        case VK_TAB:     row = 4; col = 7; break;

        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:  row = 4; col = 4; break;

        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL: row = 2; col = 4; break;

        case VK_LEFT:    row = 4; col = 5; break;
        case VK_RIGHT:   row = 4; col = 7; break;
        case VK_DOWN:    row = 4; col = 6; break;
        case VK_UP:      row = 4; col = 3; break;

        case '0': case VK_NUMPAD0: row = 7; col = 2; break;
        case '1': case VK_NUMPAD1: row = 0; col = 5; break;
        case '2': case VK_NUMPAD2: row = 2; col = 6; break;

        case '3': {
            /* Touche '3' du clavier :
             * Sur clavier AZERTY : sans Shift c'est '"' (guillemet).
             * Sur clavier QWERTY : sans Shift c'est '3'.
             * Pour permettre la saisie directe de '"' et '3' :
             */
            bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (shift_down) {
                /* Touche 3 */
                row = 0; col = 7;
            } else {
                /* Sur un clavier français, la touche 3 non décalée est le guillemet " */
                row = 3; col = 7;
                keyboard_set_key(4, 4, pressed); /* Maintient Shift pour produire '"' sur Oric */
            }
            break;
        }
        case VK_NUMPAD3: row = 0; col = 7; break;

        case '4': case VK_NUMPAD4: row = 2; col = 3; break;
        case '5': case VK_NUMPAD5: row = 0; col = 2; break;
        case '6': case VK_NUMPAD6: row = 2; col = 1; break;
        case '7': case VK_NUMPAD7: row = 0; col = 0; break;
        case '8': case VK_NUMPAD8: row = 7; col = 0; break;
        case '9': case VK_NUMPAD9: row = 3; col = 1; break;

        case 'A': row = 6; col = 5; break;
        case 'B': row = 2; col = 2; break;
        case 'C': row = 2; col = 7; break;
        case 'D': row = 1; col = 7; break;
        case 'E': row = 6; col = 3; break;
        case 'F': row = 1; col = 3; break;
        case 'G': row = 6; col = 2; break;
        case 'H': row = 6; col = 1; break;
        case 'I': row = 5; col = 1; break;
        case 'J': row = 1; col = 0; break;
        case 'K': row = 3; col = 0; break;
        case 'L': row = 7; col = 1; break;
        case 'M': row = 2; col = 0; break;
        case 'N': row = 0; col = 1; break;
        case 'O': row = 5; col = 2; break;
        case 'P': row = 5; col = 3; break;
        case 'Q': row = 1; col = 6; break;
        case 'R': row = 1; col = 2; break;
        case 'S': row = 6; col = 6; break;
        case 'T': row = 1; col = 1; break;
        case 'U': row = 5; col = 0; break;
        case 'V': row = 0; col = 3; break;
        case 'W': row = 6; col = 7; break;
        case 'X': row = 0; col = 6; break;
        case 'Y': row = 6; col = 0; break;
        case 'Z': row = 2; col = 5; break;

        case VK_OEM_COMMA:  row = 4; col = 1; break;
        case VK_OEM_PERIOD: row = 4; col = 2; break;
        case VK_DECIMAL:    row = 4; col = 2; break;
        case VK_OEM_1:      row = 3; col = 2; break;
        case VK_OEM_MINUS:  row = 3; col = 3; break;
        case VK_SUBTRACT:   row = 3; col = 3; break;
        case VK_OEM_2:      row = 7; col = 3; break;
        case VK_DIVIDE:     row = 7; col = 3; break;
        case VK_OEM_PLUS:   row = 7; col = 7; break;
        case VK_ADD:        row = 7; col = 7; break;
        case VK_MULTIPLY:   row = 7; col = 0; break;
        case VK_OEM_4:      row = 5; col = 7; break;
        case VK_OEM_6:      row = 5; col = 6; break;
        case VK_OEM_5:      row = 3; col = 6; break;
        case VK_OEM_7:      row = 3; col = 7; break; /* Touche ' / " */
        case VK_OEM_3:      row = 0; col = 5; break;
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
