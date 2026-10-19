#ifndef CPU_H
#define CPU_H

#include "common.h"

/* --- 6502 CPU --- */
typedef struct {
    uint16_t pc;
    uint8_t a, x, y, sp, p;
    uint64_t cycles;
    bool irq_pending;
    bool nmi_pending;
} CPU6502;

extern CPU6502 g_cpu;

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

void cpu_reset(void);
void cpu_step(void);
void cpu_push(uint8_t val);
uint8_t cpu_pop(void);

#endif /* CPU_H */
