#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "common.h"

/* --- Matrice Clavier Oric (8 lignes x 8 colonnes) --- */
extern uint8_t g_key_matrix[8];

void keyboard_reset(void);
void keyboard_set_key(int row, int col, bool pressed);
void keyboard_handle_vk(UINT vk, bool pressed);
uint8_t keyboard_read_bit(uint8_t row, uint8_t col_mask);

/* Saisie automatique fluide (Auto-CLOAD & commandes) */
void typein_queue(const char *str);
void typein_step(uint32_t cycles);

#endif /* KEYBOARD_H */
