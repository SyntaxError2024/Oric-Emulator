#ifndef TAPE_H
#define TAPE_H

#include "common.h"

/* --- État du lecteur de cassette Oric (.TAP) --- */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t byte_pos;
    int bit_in_byte;
    uint32_t bit_cycles_left;
    char current_name[32];
    bool active;
    bool auto_cload_triggered;
    bool started;
} TapeState;

extern TapeState g_tape;

void tape_init(void);
void tape_reset(void);
bool tape_load_file(const wchar_t *filename);

/* Hooks ROM pour chargement fiable et instantané */
bool tape_hook_byte(uint8_t *out_byte);
bool tape_hook_sync(void);

/* Émulation temporelle de la ligne CB1 vers le VIA 6522 */
void tape_step(uint32_t cycles);

#endif /* TAPE_H */
