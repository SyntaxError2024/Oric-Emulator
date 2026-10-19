#include "keyboard.h"

uint8_t g_key_matrix[8] = {0};

/* --- Buffer de saisie automatique (Auto-CLOAD & commandes) --- */
static char g_typein_buffer[64] = {0};
static int g_typein_pos = 0;
static int g_typein_len = 0;
static uint32_t g_typein_timer = 0;
static int g_typein_row = -1;
static int g_typein_col = -1;

void keyboard_reset(void) {
    memset(g_key_matrix, 0, sizeof(g_key_matrix));
    g_typein_pos = 0;
    g_typein_len = 0;
    g_typein_timer = 0;
    g_typein_row = -1;
    g_typein_col = -1;
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
        case '3':
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                keyboard_set_key(4, 4, pressed);
                row = 3; col = 7; /* " guillemet */
            } else {
                row = 0; col = 7; /* 3 */
            }
            break;
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
        case VK_OEM_7:      row = 3; col = 7; break;
        case VK_OEM_3:      row = 0; col = 5; break;
    }

    if (row >= 0 && col >= 0) {
        keyboard_set_key(row, col, pressed);
    }
}

void typein_queue(const char *str) {
    if (!str) return;
    int len = (int)strlen(str);
    if (len >= 60) len = 60;
    memcpy(g_typein_buffer, str, len);
    g_typein_buffer[len] = 0;
    g_typein_len = len;
    g_typein_pos = 0;
    g_typein_timer = 100000; /* Pause de 100ms avant la première touche */
    g_typein_row = -1;
    g_typein_col = -1;
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
        g_typein_row = -1;
        g_typein_col = -1;
        g_typein_timer = 50000; /* Pause de 50ms entre les touches */
        g_typein_pos++;
        return;
    }

    char ch = g_typein_buffer[g_typein_pos];
    int row = -1, col = -1;
    switch (ch) {
        case 'c': case 'C': row = 2; col = 7; break;
        case 'l': case 'L': row = 7; col = 1; break;
        case 'o': case 'O': row = 5; col = 2; break;
        case 'a': case 'A': row = 6; col = 5; break;
        case 'd': case 'D': row = 1; col = 7; break;
        case 'r': case 'R': row = 1; col = 2; break;
        case 'u': case 'U': row = 5; col = 0; break;
        case 'n': case 'N': row = 0; col = 1; break;
        case '"':           row = 3; col = 7; break;
        case '\r': case '\n': row = 7; col = 5; break;
    }

    if (row >= 0 && col >= 0) {
        g_typein_row = row;
        g_typein_col = col;
        keyboard_set_key(row, col, true);
        g_typein_timer = 60000; /* Touche maintenue 60ms */
    } else {
        g_typein_pos++;
    }
}
