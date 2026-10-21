#include "ay38912.h"
#include "via.h"

AY38912 g_ay;

/* Table des amplitudes logarithmiques du DAC AY-3-8912 (16 niveaux) */
static const int AY_VOL_TABLE[16] = {
    0, 218, 309, 437, 618, 874, 1236, 1748,
    2472, 3496, 4944, 6992, 9888, 13984, 19776, 28000
};

void ay_reset(void) {
    memset(&g_ay.regs, 0, sizeof(g_ay.regs));
    g_ay.regs[7] = 0xFF; /* Par défaut, tonalités et bruit désactivés */
    g_ay.selected_reg = 0;
    g_ay.noise_lfsr = 1;
    g_ay.noise_period = 1;
    g_ay.noise_counter = 0;
    g_ay.noise_output = 0;
    g_ay.env_period = 1;
    g_ay.env_counter = 0;
    g_ay.env_step = 0;
    g_ay.env_volume = 0;
    g_ay.env_holding = true;
    g_ay.env_attack = false;

    for (int i = 0; i < 3; i++) {
        g_ay.tone_period[i] = 0;
        g_ay.tone_counter[i] = 0;
        g_ay.tone_output[i] = 0;
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
            if (g_ay.noise_period == 0) g_ay.noise_period = 1;
            break;
        case 11:
        case 12:
            g_ay.env_period = (g_ay.regs[11] | ((uint32_t)g_ay.regs[12] << 8));
            if (g_ay.env_period == 0) g_ay.env_period = 1;
            break;
        case 13: {
            /* L'écriture dans le registre 13 réinitialise le générateur d'enveloppe */
            g_ay.env_counter = 0;
            g_ay.env_step = 0;
            g_ay.env_holding = false;
            g_ay.env_attack = (val & 0x04) != 0;
            g_ay.env_volume = g_ay.env_attack ? 0 : 15;
            break;
        }
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
        /* Verrouillage de l'adresse du registre sélectionné */
        g_ay.selected_reg = g_via.ora & 0x0F;
    } else if (!bc1 && bdir) {
        /* Écriture de la valeur dans le registre sélectionné */
        ay_write_reg(g_ay.selected_reg, g_via.ora);
    }
}

/* Progression du générateur d'enveloppe selon le registre 13 (Forme d'onde) */
static void ay_step_envelope(void) {
    if (g_ay.env_holding) return;

    uint8_t shape = g_ay.regs[13] & 0x0F;
    bool cont = (shape & 0x08) != 0;
    bool att  = g_ay.env_attack;
    bool alt  = (shape & 0x02) != 0;
    bool hold = (shape & 0x01) != 0;

    g_ay.env_step++;
    if (g_ay.env_step < 16) {
        g_ay.env_volume = att ? g_ay.env_step : (15 - g_ay.env_step);
    } else {
        if (!cont) {
            /* Formes 0 à 7 : cycle unique puis extinction */
            g_ay.env_volume = 0;
            g_ay.env_holding = true;
        } else {
            if (hold) {
                if (alt) {
                    g_ay.env_volume = att ? 0 : 15;
                } else {
                    g_ay.env_volume = att ? 15 : 0;
                }
                g_ay.env_holding = true;
            } else {
                if (alt) {
                    g_ay.env_attack = !g_ay.env_attack;
                }
                g_ay.env_step = 0;
                g_ay.env_volume = g_ay.env_attack ? 0 : 15;
            }
        }
    }
}

/* ============================================================================
 * MOTEUR AUDIO WIN32 WAVEOUT PUR (44100 Hz 16-bit mono)
 * Utilise un anneau de 4 tampons en CALLBACK_NULL pour une fluidité sans blocage
 * ============================================================================ */

void audio_init(HWND hwnd) {
    (void)hwnd;
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = AY_SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = AY_SAMPLE_RATE * 2;

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
        g_ay.waveHdr[i].dwBufferLength = AY_SAMPLES_PER_FRAME * sizeof(short);
    }

    debug_log(L"Audio WaveOut initialisé (44100 Hz 16-bit mono, ring buffer)");
}

void audio_update(void) {
    if (!g_ay.audio_enabled) return;

    WAVEHDR *hdr = &g_ay.waveHdr[g_ay.currentBuffer];

    /* Si ce tampon a déjà été envoyé à WaveOut, vérifier s'il a terminé de jouer */
    if (hdr->dwFlags & WHDR_PREPARED) {
        if (hdr->dwFlags & WHDR_INQUEUE) {
            /* Le périphérique audio n'a pas encore fini de le lire, on saute pour éviter le blocage */
            return;
        }
        waveOutUnprepareHeader(g_ay.hWaveOut, hdr, sizeof(WAVEHDR));
    }

    /* Fréquences horloge AY-3-8912 :
     * Horloge maître = 1 MHz (CPU Oric)
     * Horloge tonalité et bruit = 1 MHz / 16 = 62500 Hz
     * Horloge enveloppe = 1 MHz / 256 = 3906.25 Hz
     * Accumulateurs en virgule fixe 16.16
     */
    const uint32_t tone_step = (uint32_t)((62500ULL << 16) / AY_SAMPLE_RATE);
    const uint32_t env_step_inc = (uint32_t)((1000000ULL << 16) / (256 * AY_SAMPLE_RATE));

    short *buf = g_ay.audioBuffer[g_ay.currentBuffer];

    for (int i = 0; i < AY_SAMPLES_PER_FRAME; i++) {
        /* 1. Avancement du générateur d'enveloppe */
        uint32_t env_p = g_ay.env_period ? g_ay.env_period : 1;
        uint32_t env_target = env_p << 16;
        g_ay.env_counter += env_step_inc;
        while (g_ay.env_counter >= env_target) {
            g_ay.env_counter -= env_target;
            ay_step_envelope();
        }

        /* 2. Avancement du générateur de bruit (LFSR 17 bits : x^17 + x^14 + 1) */
        uint32_t noise_p = g_ay.noise_period ? g_ay.noise_period : 1;
        uint32_t noise_target = noise_p << 16;
        g_ay.noise_counter += tone_step;
        while (g_ay.noise_counter >= noise_target) {
            g_ay.noise_counter -= noise_target;
            uint32_t bit = (g_ay.noise_lfsr ^ (g_ay.noise_lfsr >> 3)) & 1;
            g_ay.noise_lfsr = (g_ay.noise_lfsr >> 1) | (bit << 16);
            g_ay.noise_output = (g_ay.noise_lfsr & 1);
        }

        /* 3. Avancement des 3 oscillateurs de tonalité et mixage */
        int sample = 0;
        for (int ch = 0; ch < 3; ch++) {
            uint32_t per = g_ay.tone_period[ch] ? g_ay.tone_period[ch] : 1;
            uint32_t tone_target = per << 16;
            g_ay.tone_counter[ch] += tone_step;
            while (g_ay.tone_counter[ch] >= tone_target) {
                g_ay.tone_counter[ch] -= tone_target;
                g_ay.tone_output[ch] ^= 1;
            }

            /* Registre 7 : Mixer (0 = actif, 1 = désactivé) */
            bool tone_dis  = (g_ay.regs[7] >> ch) & 1;
            bool noise_dis = (g_ay.regs[7] >> (ch + 3)) & 1;

            bool tone_val  = tone_dis ? 1 : g_ay.tone_output[ch];
            bool noise_val = noise_dis ? 1 : g_ay.noise_output;
            bool ch_out    = tone_val & noise_val;

            /* Volume (fixe 0..15 ou enveloppe si bit 4 = 1) */
            int vol;
            if (g_ay.regs[8 + ch] & 0x10) {
                vol = g_ay.env_volume;
            } else {
                vol = g_ay.regs[8 + ch] & 0x0F;
            }
            if (vol > 15) vol = 15;
            if (vol < 0)  vol = 0;

            int amp = AY_VOL_TABLE[vol];
            /* Forme d'onde centrée pour éliminer toute composante continue */
            int ch_sample = ch_out ? (amp / 3) : -(amp / 3);
            sample += ch_sample;
        }

        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        buf[i] = (short)sample;
    }

    hdr->dwBufferLength = AY_SAMPLES_PER_FRAME * sizeof(short);
    hdr->dwFlags = 0;
    waveOutPrepareHeader(g_ay.hWaveOut, hdr, sizeof(WAVEHDR));
    waveOutWrite(g_ay.hWaveOut, hdr, sizeof(WAVEHDR));

    g_ay.currentBuffer = (g_ay.currentBuffer + 1) % AY_NUM_BUFFERS;
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
    debug_log(L"Audio WaveOut arrêté proprement");
}
