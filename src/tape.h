#ifndef TAPE_H
#define TAPE_H

#include "common.h"

/* --- État du lecteur de cassette Oric (.TAP) --- */
typedef struct {
    uint8_t *pulse_buf;        /* Flux complet d'impulsions (1 = bit 1, 0 = bit 0) */
    size_t pulse_count;        /* Nombre total d'impulsions */
    size_t pulse_pos;          /* Position courante de lecture de l'impulsion */
    int32_t bit_cycles_left;   /* Cycles CPU restants pour l'impulsion en cours */
    char current_name[32];     /* Nom du programme extrait du premier en-tête .TAP */
    bool loaded;               /* Indique si un fichier .TAP valide est chargé */
    bool auto_cload_triggered; /* Indique si la saisie auto CLOAD "" a déjà eu lieu */
    uint8_t last_bit_t2h;      /* Valeur Timer 2 High stable pour la ROM (0xFE = bit 1, 0xFD = bit 0) */
} TapeState;

extern TapeState g_tape;

void tape_init(void);
void tape_reset(void);
bool tape_load_file(const wchar_t *filename);
bool tape_load_file_quiet(const wchar_t *filename);

/* Émulation matérielle du signal CB1 et du Timer 2 vers le VIA 6522 */
void tape_step(uint32_t cycles);

#endif /* TAPE_H */
