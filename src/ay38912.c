#include "ay38912.h"
#include "via.h"

AY38912 g_ay;

/* Table DAC logarithmique standard du processeur sonore AY-3-8912 */
static const int16_t AY_DAC_VOL[16] = {
    0, 220, 315, 450, 640, 910, 1300, 1850,
    2600, 3700, 5200, 7400, 10500, 15000, 21500, 30000
};

void ay_reset(void) {
    memset(g_ay.regs, 0, sizeof(g_ay.regs));
    g_ay.selected_reg = 0;
    g_ay.noise_lfsr = 1;
    g_ay.noise_period = 0;
    g_ay.noise_counter = 0;
    g_ay.noise_output = 1;

    g_ay.env_period = 0;
    g_ay.env_counter = 0;
    g_ay.env_step = 0;
    g_ay.env_volume = 0;
    g_ay.env_shape = 0;
    g_ay.env_holding = false;

    g_ay.sample_accum = 0;

    for (int i = 0; i < 3; i++) {
        g_ay.tone_period[i] = 0;
        g_ay.tone_counter[i] = 0;
        g_ay.tone_output[i] = 1;
    }
}

void ay_write_reg(uint8_t reg, uint8_t val) {
    if (reg >= 16) return;
    g_ay.regs[reg] = val;

    switch (reg) {
        case 0:
        case 1:
            g_ay.tone_period[0] = (g_ay.regs[0] | ((uint32_t)(g_ay.regs[1] & 0x0F) << 8));
            break;
        case 2:
        case 3:
            g_ay.tone_period[1] = (g_ay.regs[2] | ((uint32_t)(g_ay.regs[3] & 0x0F) << 8));
            break;
        case 4:
        case 5:
            g_ay.tone_period[2] = (g_ay.regs[4] | ((uint32_t)(g_ay.regs[5] & 0x0F) << 8));
            break;
        case 6:
            g_ay.noise_period = (g_ay.regs[6] & 0x1F);
            break;
        case 11:
        case 12:
            g_ay.env_period = (g_ay.regs[11] | ((uint32_t)g_ay.regs[12] << 8));
            break;
        case 13:
            g_ay.env_shape = val & 0x0F;
            g_ay.env_step = 0;
            g_ay.env_holding = false;
            break;
    }
}

uint8_t ay_read_reg(uint8_t reg) {
    if (reg >= 16) return 0xFF;
    return g_ay.regs[reg];
}

void ay_check_control(void) {
    bool bc1  = (g_via.pcr & 0x0E) == 0x0E;
    bool bdir = (g_via.pcr & 0xE0) == 0xE0;

    if (bc1 && bdir) {
        /* Sélection du registre actif (adresse) */
        g_ay.selected_reg = g_via.ora & 0x0F;
    } else if (!bc1 && bdir) {
        /* Écriture dans le registre actif */
        ay_write_reg(g_ay.selected_reg, g_via.ora);
    }
}

static void advance_envelope(void) {
    if (g_ay.env_holding) return;

    bool attack = (g_ay.env_shape & 0x04) != 0;
    g_ay.env_volume = attack ? g_ay.env_step : (15 - g_ay.env_step);

    g_ay.env_step++;
    if (g_ay.env_step > 15) {
        bool cont = (g_ay.env_shape & 0x08) != 0;
        if (!cont) {
            g_ay.env_volume = 0;
            g_ay.env_holding = true;
        } else {
            bool alt  = (g_ay.env_shape & 0x02) != 0;
            bool hold = (g_ay.env_shape & 0x01) != 0;
            if (hold) {
                if (alt) attack = !attack;
                g_ay.env_volume = attack ? 15 : 0;
                g_ay.env_holding = true;
            } else {
                if (alt) {
                    g_ay.env_shape ^= 0x04;
                }
                g_ay.env_step = 0;
            }
        }
    }
}

/* ============================================================================
 * AUDIO WIN32 (WAVEOUT) PUR
 * ============================================================================ */
void audio_init(HWND hwnd) {
    (void)hwnd;
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 22050;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = 44100;

    if (waveOutOpen(&g_ay.hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        debug_log(L"Avertissement : Impossible d'ouvrir le périphérique audio WaveOut");
        g_ay.audio_enabled = false;
        return;
    }

    g_ay.audio_enabled = true;
    g_ay.currentBuffer = 0;

    for (int i = 0; i < AY_NUM_BUFFERS; i++) {
        memset(&g_ay.waveHdr[i], 0, sizeof(WAVEHDR));
        g_ay.waveHdr[i].lpData = (LPSTR)g_ay.audioBuffer[i];
        g_ay.waveHdr[i].dwBufferLength = sizeof(g_ay.audioBuffer[i]);
        g_ay.waveHdr[i].dwFlags = 0;
    }
    debug_log(L"Audio WaveOut initialisé (22050 Hz 16-bit mono, 4 tampons annulaires)");
}

void audio_update(void) {
    if (!g_ay.audio_enabled) return;

    AY38912 *ay = &g_ay;
    WAVEHDR *hdr = &ay->waveHdr[ay->currentBuffer];

    /* Si le tampon est encore en cours de lecture par le pilote, ne pas le corrompre */
    if (hdr->dwFlags & WHDR_PREPARED) {
        if (!(hdr->dwFlags & WHDR_DONE)) {
            return;
        }
        waveOutUnprepareHeader(ay->hWaveOut, hdr, sizeof(WAVEHDR));
    }

    short *buf = ay->audioBuffer[ay->currentBuffer];

    /* Synthèse pour la durée de la trame (~441 échantillons = 20ms à 22050 Hz) */
    for (int i = 0; i < AY_BUFFER_SAMPLES; i++) {
        /* Horloge PSG AY-3-8912 : 1 MHz divisé par 16 = 62 500 Hz */
        ay->sample_accum += 62500;
        while (ay->sample_accum >= 22050) {
            ay->sample_accum -= 22050;

            /* Mise à jour des compteurs de tonalité (Canaux A, B, C) */
            for (int ch = 0; ch < 3; ch++) {
                ay->tone_counter[ch]++;
                uint32_t tp = ay->tone_period[ch];
                if (tp > 0 && ay->tone_counter[ch] >= tp) {
                    ay->tone_counter[ch] = 0;
                    ay->tone_output[ch] ^= 1;
                }
            }

            /* Mise à jour du générateur de bruit (LFSR 17 bits) */
            ay->noise_counter++;
            uint32_t np = ay->noise_period ? ay->noise_period : 1;
            if (ay->noise_counter >= np) {
                ay->noise_counter = 0;
                uint32_t bit = (ay->noise_lfsr ^ (ay->noise_lfsr >> 3)) & 1;
                ay->noise_lfsr = (ay->noise_lfsr >> 1) | (bit << 16);
                ay->noise_output = (uint8_t)(ay->noise_lfsr & 1);
            }

            /* Mise à jour du générateur d'enveloppe */
            ay->env_counter++;
            uint32_t ep = ay->env_period ? ay->env_period : 1;
            if (ay->env_counter >= ep) {
                ay->env_counter = 0;
                advance_envelope();
            }
        }

        /* Mixage des 3 canaux selon le registre 7 (Mixer Control) et calcul de volume */
        int mixed_sample = 0;
        for (int ch = 0; ch < 3; ch++) {
            bool tone_en = ((ay->regs[7] >> ch) & 1) == 0;
            bool noise_en = ((ay->regs[7] >> (ch + 3)) & 1) == 0;

            uint8_t out_bit = (tone_en ? ay->tone_output[ch] : 1) & (noise_en ? ay->noise_output : 1);

            uint8_t vol;
            if (ay->regs[8 + ch] & 0x10) {
                vol = ay->env_volume;
            } else {
                vol = ay->regs[8 + ch] & 0x0F;
            }

            if (vol > 0) {
                int chan_val = out_bit ? AY_DAC_VOL[vol] : -AY_DAC_VOL[vol];
                mixed_sample += chan_val / 3;
            }
        }

        if (mixed_sample > 32767) mixed_sample = 32767;
        else if (mixed_sample < -32768) mixed_sample = -32768;

        buf[i] = (short)mixed_sample;
    }

    hdr->dwFlags = 0;
    waveOutPrepareHeader(ay->hWaveOut, hdr, sizeof(WAVEHDR));
    waveOutWrite(ay->hWaveOut, hdr, sizeof(WAVEHDR));

    ay->currentBuffer = (ay->currentBuffer + 1) % AY_NUM_BUFFERS;
}

void audio_shutdown(void) {
    if (!g_ay.audio_enabled) return;
    waveOutReset(g_ay.hWaveOut);
    for (int i = 0; i < AY_NUM_BUFFERS; i++) {
        if (g_ay.waveHdr[i].dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(g_ay.hWaveOut, &g_ay.waveHdr[i], sizeof(WAVEHDR));
        }
    }
    waveOutClose(g_ay.hWaveOut);
    g_ay.audio_enabled = false;
    debug_log(L"Audio WaveOut arrêté");
}
