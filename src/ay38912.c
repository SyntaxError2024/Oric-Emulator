#include "ay38912.h"
#include "via.h"

AY38912 g_ay;

void ay_reset(void) {
    memset(&g_ay.regs, 0, sizeof(g_ay.regs));
    g_ay.selected_reg = 0;
    g_ay.noise_lfsr = 1;
    g_ay.noise_period = 0;
    g_ay.env_period = 0;
    for (int i = 0; i < 3; i++) {
        g_ay.tone_period[i] = 0;
        g_ay.tone_counter[i] = 0;
        g_ay.tone_output[i] = 0;
    }
}

void ay_write_reg(uint8_t reg, uint8_t val) {
    if (reg >= 16) return;
    g_ay.regs[reg] = val;

    g_ay.tone_period[0] = (g_ay.regs[0] | ((uint32_t)(g_ay.regs[1] & 0x0F) << 8));
    g_ay.tone_period[1] = (g_ay.regs[2] | ((uint32_t)(g_ay.regs[3] & 0x0F) << 8));
    g_ay.tone_period[2] = (g_ay.regs[4] | ((uint32_t)(g_ay.regs[5] & 0x0F) << 8));
    g_ay.noise_period   = (g_ay.regs[6] & 0x1F) * 2;
    g_ay.env_period     = (g_ay.regs[11] | ((uint32_t)g_ay.regs[12] << 8));
}

uint8_t ay_read_reg(uint8_t reg) {
    if (reg >= 16) return 0xFF;
    return g_ay.regs[reg];
}

void ay_check_control(void) {
    bool bc1  = (g_via.pcr & 0x0E) == 0x0E;
    bool bdir = (g_via.pcr & 0xE0) == 0xE0;

    if (bc1 && bdir) {
        /* Latch Register Address */
        g_ay.selected_reg = g_via.ora & 0x0F;
    } else if (!bc1 && bdir) {
        /* Write to selected register */
        ay_write_reg(g_ay.selected_reg, g_via.ora);
    }
}

/* ============================================================================
 * AUDIO WIN32 (WAVEOUT) PUR
 * ============================================================================ */
void audio_init(HWND hwnd) {
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 22050;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = 44100;

    if (waveOutOpen(&g_ay.hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)hwnd, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR) {
        debug_log(L"Avertissement : Impossible d'ouvrir le périphérique audio WaveOut");
        g_ay.audio_enabled = false;
        return;
    }

    g_ay.audio_enabled = true;
    for (int i = 0; i < 2; i++) {
        memset(&g_ay.waveHdr[i], 0, sizeof(WAVEHDR));
        g_ay.waveHdr[i].lpData = (LPSTR)g_ay.audioBuffer[i];
        g_ay.waveHdr[i].dwBufferLength = sizeof(g_ay.audioBuffer[i]);
        waveOutPrepareHeader(g_ay.hWaveOut, &g_ay.waveHdr[i], sizeof(WAVEHDR));
    }
    debug_log(L"Audio WaveOut initialisé (22050 Hz 16-bit mono)");
}

void audio_update(void) {
    if (!g_ay.audio_enabled) return;

    AY38912 *ay = &g_ay;
    short *buf = ay->audioBuffer[ay->currentBuffer];
    int samples = 441; /* ~50Hz frame */

    for (int i = 0; i < samples; i++) {
        int sample = 0;
        for (int ch = 0; ch < 3; ch++) {
            ay->tone_counter[ch]++;
            if (ay->tone_period[ch] > 0 && ay->tone_counter[ch] >= ay->tone_period[ch]) {
                ay->tone_counter[ch] = 0;
                ay->tone_output[ch] ^= 1;
            }
            int volume = ay->regs[8 + ch] & 0x0F;
            if (volume > 0 && ay->tone_period[ch] > 0) {
                int chan_val = ay->tone_output[ch] ? (volume * 100) : -(volume * 100);
                sample += chan_val;
            }
        }
        buf[i] = (short)sample;
    }

    WAVEHDR *hdr = &ay->waveHdr[ay->currentBuffer];
    if (hdr->dwFlags & WHDR_INQUEUE) {
        waveOutUnprepareHeader(ay->hWaveOut, hdr, sizeof(WAVEHDR));
        waveOutPrepareHeader(ay->hWaveOut, hdr, sizeof(WAVEHDR));
    }
    waveOutWrite(ay->hWaveOut, hdr, sizeof(WAVEHDR));
    ay->currentBuffer ^= 1;
}

void audio_shutdown(void) {
    if (!g_ay.audio_enabled) return;
    waveOutReset(g_ay.hWaveOut);
    for (int i = 0; i < 2; i++) {
        waveOutUnprepareHeader(g_ay.hWaveOut, &g_ay.waveHdr[i], sizeof(WAVEHDR));
    }
    waveOutClose(g_ay.hWaveOut);
    g_ay.audio_enabled = false;
    debug_log(L"Audio WaveOut arrêté");
}
