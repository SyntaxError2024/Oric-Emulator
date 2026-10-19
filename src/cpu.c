#include "cpu.h"
#include "memory.h"
#include "via.h"
#include "tape.h"
#include "keyboard.h"

CPU6502 g_cpu;

static inline void cpu_set_nz(uint8_t val) {
    g_cpu.p &= ~(FLAG_N | FLAG_Z);
    if (!val) g_cpu.p |= FLAG_Z;
    if (val & 0x80) g_cpu.p |= FLAG_N;
}

void cpu_push(uint8_t val) {
    mem_write(0x0100 + (g_cpu.sp-- & 0xFF), val);
}

uint8_t cpu_pop(void) {
    return mem_read(0x0100 + (++g_cpu.sp & 0xFF));
}

void cpu_reset(void) {
    g_cpu.a = 0;
    g_cpu.x = 0;
    g_cpu.y = 0;
    g_cpu.sp = 0xFD;
    g_cpu.p = FLAG_U | FLAG_I;
    g_cpu.irq_pending = false;
    g_cpu.nmi_pending = false;
    g_cpu.cycles = 0;

    uint8_t lo = mem_read(0xFFFC);
    uint8_t hi = mem_read(0xFFFD);
    g_cpu.pc = lo | ((uint16_t)hi << 8);
    if (g_cpu.pc == 0 || g_cpu.pc == 0xFFFF) {
        g_cpu.pc = 0xF88F;
    }
    debug_log(L"CPU 6502 réinitialisé (PC=0x%04X, SP=0x%02X)", g_cpu.pc, g_cpu.sp);
}

void cpu_step(void) {
    /* Auto-CLOAD si un fichier TAP est chargé : saisie automatique de CLOAD au démarrage */
    if (g_tape.active && !g_tape.auto_cload_triggered) {
        if (g_cpu.cycles > 1200000) { /* ~1.2s après boot BASIC */
            g_tape.auto_cload_triggered = true;
            typein_queue("CLOAD\r");
            debug_log(L"Auto-CLOAD déclenché après boot BASIC");
        }
    }

    /* --- HOOKS DE CHARGEMENT K7 ORIC ATMOS --- */
    if (g_tape.active && g_tape.data) {
        /* Hook 0xE6C9: RDBYTE (Lecture d'un octet de données depuis la K7) */
        if (g_cpu.pc == 0xE6C9) {
            uint8_t b = 0;
            if (tape_hook_byte(&b)) {
                g_cpu.a = b;
                mem_write(0x2F, b);
                g_cpu.p &= ~FLAG_C;
                cpu_set_nz(b);
                uint16_t lo = cpu_pop();
                uint16_t hi = cpu_pop();
                g_cpu.pc = (lo | ((uint16_t)hi << 8)) + 1;
                g_cpu.cycles += 30;
                return;
            }
        }
        /* Hook 0xE735: WAIT_SYNC (Synchronisation sur les octets 0x16 SYN) */
        else if (g_cpu.pc == 0xE735) {
            if (tape_hook_sync()) {
                g_cpu.a = 0x16;
                mem_write(0x2F, 0x16);
                g_cpu.p &= ~FLAG_C;
                cpu_set_nz(0x16);
                uint16_t lo = cpu_pop();
                uint16_t hi = cpu_pop();
                g_cpu.pc = (lo | ((uint16_t)hi << 8)) + 1;
                g_cpu.cycles += 30;
                return;
            }
        }
        /* Hook 0xE71C: RDBIT (Lecture d'un bit de signal série K7) */
        else if (g_cpu.pc == 0xE71C) {
            uint8_t bit = 1;
            if (tape_hook_bit(&bit)) {
                if (bit) {
                    g_cpu.p |= FLAG_C;
                    g_via.t2c = 0xFE00;
                } else {
                    g_cpu.p &= ~FLAG_C;
                    g_via.t2c = 0xFD00;
                }
                g_via.ifr &= ~0x10;
                uint16_t lo = cpu_pop();
                uint16_t hi = cpu_pop();
                g_cpu.pc = (lo | ((uint16_t)hi << 8)) + 1;
                g_cpu.cycles += 30;
                return;
            }
        }
    }

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

        #define DO_ADC(expr) { \
            uint8_t val = (expr); \
            int carry = (g_cpu.p & FLAG_C) ? 1 : 0; \
            if (g_cpu.p & FLAG_D) { \
                int al = (g_cpu.a & 0x0F) + (val & 0x0F) + carry; \
                int ah = (g_cpu.a >> 4) + (val >> 4); \
                if (al > 9) { al += 6; ah++; } \
                if (ah > 9) { ah += 6; } \
                g_cpu.p &= ~FLAG_C; \
                if (ah > 0x0F) g_cpu.p |= FLAG_C; \
                g_cpu.a = ((ah & 0x0F) << 4) | (al & 0x0F); \
            } else { \
                uint16_t sum = g_cpu.a + val + carry; \
                g_cpu.p &= ~(FLAG_C | FLAG_V); \
                if (sum > 0xFF) g_cpu.p |= FLAG_C; \
                if (~(g_cpu.a ^ val) & (g_cpu.a ^ sum) & 0x80) g_cpu.p |= FLAG_V; \
                g_cpu.a = (uint8_t)sum; \
            } \
            cpu_set_nz(g_cpu.a); \
        }
        case 0x69: DO_ADC(mem_read(g_cpu.pc++)); break;
        case 0x65: DO_ADC(mem_read(mem_read(g_cpu.pc++))); break;
        case 0x75: DO_ADC(mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0x6D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ADC(mem_read(a)); break; }
        case 0x7D: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ADC(mem_read(a + g_cpu.x)); break; }
        case 0x79: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_ADC(mem_read(a + g_cpu.y)); break; }
        case 0x61: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_ADC(mem_read(a)); break; }
        case 0x71: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_ADC(mem_read(a)); break; }

        #define DO_SBC(expr) { \
            uint8_t val = (expr); \
            int borrow = (g_cpu.p & FLAG_C) ? 0 : 1; \
            int diff = g_cpu.a - val - borrow; \
            if (g_cpu.p & FLAG_D) { \
                int al = (g_cpu.a & 0x0F) - (val & 0x0F) - borrow; \
                int ah = (g_cpu.a >> 4) - (val >> 4); \
                if (al < 0) { al -= 6; ah--; } \
                if (ah < 0) { ah -= 6; } \
                g_cpu.p &= ~FLAG_C; \
                if (diff >= 0) g_cpu.p |= FLAG_C; \
                g_cpu.a = ((ah & 0x0F) << 4) | (al & 0x0F); \
            } else { \
                g_cpu.p &= ~(FLAG_C | FLAG_V); \
                if (diff >= 0) g_cpu.p |= FLAG_C; \
                if ((g_cpu.a ^ val) & (g_cpu.a ^ diff) & 0x80) g_cpu.p |= FLAG_V; \
                g_cpu.a = (uint8_t)diff; \
            } \
            cpu_set_nz(g_cpu.a); \
        }
        case 0xE9: case 0xEB: DO_SBC(mem_read(g_cpu.pc++)); break;
        case 0xE5: DO_SBC(mem_read(mem_read(g_cpu.pc++))); break;
        case 0xF5: DO_SBC(mem_read((mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF)); break;
        case 0xED: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_SBC(mem_read(a)); break; }
        case 0xFD: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_SBC(mem_read(a + g_cpu.x)); break; }
        case 0xF9: { uint16_t a = mem_read(g_cpu.pc++); a |= ((uint16_t)mem_read(g_cpu.pc++) << 8); DO_SBC(mem_read(a + g_cpu.y)); break; }
        case 0xE1: { uint8_t zp = (mem_read(g_cpu.pc++) + g_cpu.x) & 0xFF; uint16_t a = mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8); DO_SBC(mem_read(a)); break; }
        case 0xF1: { uint8_t zp = mem_read(g_cpu.pc++); uint16_t a = (mem_read(zp) | ((uint16_t)mem_read((zp+1)&0xFF) << 8)) + g_cpu.y; DO_SBC(mem_read(a)); break; }

        #define DO_CMP(reg, expr) { \
            uint8_t val = (expr); \
            uint16_t r = (uint16_t)reg - val; \
            g_cpu.p &= ~(FLAG_C | FLAG_Z | FLAG_N); \
            if (reg >= val) g_cpu.p |= FLAG_C; \
            if (reg == val) g_cpu.p |= FLAG_Z; \
            if (r & 0x80) g_cpu.p |= FLAG_N; \
        }
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
