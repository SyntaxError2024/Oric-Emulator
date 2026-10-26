#include "via.h"
#include "ay38912.h"
#include "keyboard.h"
#include "tape.h"
#include "cpu.h"

VIA6522 g_via;

void via_reset(void) {
    memset(&g_via, 0, sizeof(g_via));
    g_via.t1l = 0x2710;
    g_via.t1c = g_via.t1l;
    g_via.t2l = 0xFFFF;
    g_via.t2c = 0xFFFF;
    g_via.ier = 0x40;
    g_via.pcr = 0xCC;
    g_via.t1_active = true;
    g_via.t2_active = true;
}

uint8_t via_read(uint8_t reg) {
    switch (reg) {
        case 0x00: {
            /* Lecture ORB : acquitte CB1 et lit la ligne de retour clavier PB3 */
            g_via.ifr &= ~0x10;
            cpu_update_irq();
            uint8_t row = g_via.orb & 0x07;
            uint8_t col_mask = (~g_ay.regs[14]) & 0xFF;
            uint8_t key_bit = keyboard_read_bit(row, col_mask);
            return (g_via.orb & ~0x08) | key_bit;
        }
        case 0x01:
        case 0x0F: {
            bool bc1  = (g_via.pcr & 0x0E) == 0x0E;
            bool bdir = (g_via.pcr & 0xE0) == 0xE0;
            if (bc1 && !bdir) {
                return ay_read_reg(g_ay.selected_reg & 0x0F);
            }
            return (g_via.ora & g_via.ddra) | (~g_via.ddra & 0xFF);
        }
        case 0x02: return g_via.ddrb;
        case 0x03: return g_via.ddra;
        case 0x04:
            g_via.ifr &= ~0x40;
            cpu_update_irq();
            return g_via.t1c & 0xFF;
        case 0x05:
            return (g_via.t1c >> 8) & 0xFF;
        case 0x06: return g_via.t1l & 0xFF;
        case 0x07: return (g_via.t1l >> 8) & 0xFF;
        case 0x08:
            g_via.ifr &= ~0x20;
            cpu_update_irq();
            return g_via.t2c & 0xFF;
        case 0x09:
            if ((g_via.orb & 0x40) && g_tape.loaded && g_tape.last_bit_t2h != 0) {
                return g_tape.last_bit_t2h;
            }
            return (g_via.t2c >> 8) & 0xFF;
        case 0x0A: return g_via.sr;
        case 0x0B: return g_via.acr;
        case 0x0C: return g_via.pcr;
        case 0x0D:
            return g_via.ifr | ((g_via.ifr & g_via.ier & 0x7F) ? 0x80 : 0x00);
        case 0x0E: return g_via.ier | 0x80;
    }
    return 0xFF;
}

void via_write(uint8_t reg, uint8_t val) {
    switch (reg) {
        case 0x00:
            g_via.orb = val;
            g_via.ifr &= ~0x10; /* L'écriture dans ORB acquitte CB1 */
            break;
        case 0x01:
        case 0x0F:
            g_via.ora = val;
            ay_check_control();
            break;
        case 0x02: g_via.ddrb = val; break;
        case 0x03: g_via.ddra = val; break;
        case 0x04:
            g_via.t1l = (g_via.t1l & 0xFF00) | val;
            break;
        case 0x05:
            g_via.t1l = (g_via.t1l & 0x00FF) | ((uint16_t)val << 8);
            g_via.t1c = g_via.t1l;
            g_via.ifr &= ~0x40;
            g_via.t1_active = true;
            break;
        case 0x06:
            g_via.t1l = (g_via.t1l & 0xFF00) | val;
            break;
        case 0x07:
            g_via.t1l = (g_via.t1l & 0x00FF) | ((uint16_t)val << 8);
            g_via.ifr &= ~0x40; /* Écriture dans le verrou haut T1 acquitte IFR6 (spécification 6522) */
            break;
        case 0x08:
            g_via.t2l = (g_via.t2l & 0xFF00) | val;
            break;
        case 0x09:
            g_tape.last_bit_t2h = 0;
            g_via.t2l = (g_via.t2l & 0x00FF) | ((uint16_t)val << 8);
            if ((g_via.t2l & 0xFF) == 0) g_via.t2l |= 0xFF;
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
    cpu_update_irq();
}

void via_step(uint32_t cycles) {
    if (g_via.t1_active) {
        if (g_via.t1c <= cycles) {
            g_via.ifr |= 0x40;
            /* Mode continu : rechargement depuis le verrou t1l.
             * Si t1l est temporairement inférieur à 100 cycles (ex: écriture en deux temps
             * du mot 16-bit 0x2710 où le registre bas 0x10 est écrit avant le registre haut 0x27),
             * on évite d'emballer le CPU à 62 kHz d'interruptions en rechargeant 10000 cycles (100 Hz). */
            uint32_t reload = (g_via.t1l < 100) ? 10000 : g_via.t1l;
            g_via.t1c = (uint16_t)(reload - ((cycles - g_via.t1c) % reload));
        } else {
            g_via.t1c -= cycles;
        }
    }

    if (g_via.t2_active) {
        if (g_via.t2c <= cycles) {
            g_via.ifr |= 0x20;
            g_via.t2c = (uint16_t)(0xFFFF - ((cycles - g_via.t2c) % 0xFFFF));
        } else {
            g_via.t2c -= cycles;
        }
    }

    tape_step(cycles);
    typein_step(cycles);

    cpu_update_irq();
}
