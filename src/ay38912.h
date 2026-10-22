#ifndef AY38912_H
#define AY38912_H

#include "common.h"

#define AY_NUM_BUFFERS 4
#define AY_BUFFER_SAMPLES 441 /* 441 échantillons à 22050 Hz = ~20ms = 1 trame 50Hz */

/* --- AY-3-8912 PSG (Générateur Sonore & Port Clavier) --- */
typedef struct {
    uint8_t regs[16];
    uint8_t selected_reg;

    /* Tonalités (Canaux A, B, C) */
    uint32_t tone_period[3];
    uint32_t tone_counter[3];
    uint8_t tone_output[3];

    /* Bruit blanc (LFSR 17 bits) */
    uint32_t noise_period;
    uint32_t noise_counter;
    uint32_t noise_lfsr;
    uint8_t noise_output;

    /* Enveloppe */
    uint32_t env_period;
    uint32_t env_counter;
    uint8_t env_step;
    uint8_t env_volume;
    uint8_t env_shape;
    bool env_holding;

    /* Accumulateur d'échantillonnage */
    uint32_t sample_accum;

    /* Audio Win32 WaveOut */
    HWAVEOUT hWaveOut;
    WAVEHDR waveHdr[AY_NUM_BUFFERS];
    short audioBuffer[AY_NUM_BUFFERS][AY_BUFFER_SAMPLES];
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
