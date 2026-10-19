#ifndef AY38912_H
#define AY38912_H

#include "common.h"

/* --- AY-3-8912 PSG (Générateur Sonore & Port Clavier) --- */
typedef struct {
    uint8_t regs[16];
    uint8_t selected_reg;
    uint32_t tone_period[3];
    uint32_t tone_counter[3];
    uint8_t tone_output[3];
    uint32_t noise_period;
    uint32_t noise_counter;
    uint32_t noise_lfsr;
    uint8_t noise_output;
    uint32_t env_period;
    uint32_t env_counter;
    uint8_t env_step;
    uint8_t env_volume;
    HWAVEOUT hWaveOut;
    WAVEHDR waveHdr[2];
    short audioBuffer[2][1024];
    int currentBuffer;
    bool audio_enabled;
} AY38912;

extern AY38912 g_ay;

void ay_reset(void);
void ay_write_reg(uint8_t reg, uint8_t val);
uint8_t ay_read_reg(uint8_t reg);
void ay_check_control(void);

void audio_init(HWND hwnd);
void audio_update(void);
void audio_shutdown(void);

#endif /* AY38912_H */
