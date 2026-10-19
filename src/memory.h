#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

extern uint8_t g_ram[65536];
extern uint8_t g_rom[16384];
extern bool g_rom_loaded;
extern uint8_t g_microdis_rom[8192];
extern bool g_microdis_loaded;
extern uint8_t g_microdis_control;

void memory_reset(void);
uint8_t mem_read(uint16_t addr);
void mem_write(uint16_t addr, uint8_t val);

void oric_load_rom(const wchar_t *path);
void oric_load_microdis_rom(const wchar_t *path);

#endif /* MEMORY_H */
