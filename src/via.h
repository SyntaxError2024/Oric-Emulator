#ifndef VIA_H
#define VIA_H

#include "common.h"

/* --- VIA 6522 --- */
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

extern VIA6522 g_via;

void via_reset(void);
void via_step(uint32_t cycles);
uint8_t via_read(uint8_t reg);
void via_write(uint8_t reg, uint8_t val);

#endif /* VIA_H */
