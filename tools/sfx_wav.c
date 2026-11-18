/* Renders the DirectSound effects (sound.c's real mixer) to WAV files for listening on a PC.
   Build from the repo root:  gcc -O1 -w -Isrc -o build/sfx_wav tools/sfx_wav.c src/tables.c
   Run:  build/sfx_wav build/sfx_      (writes build/sfx_<name>.wav, 10512 Hz mono 8-bit) */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static unsigned char fake_io[0x400];
#define REG16(a) (*(volatile unsigned short *)&fake_io[(uintptr_t)(a) & 0x3FE])
#define REG32(a) (*(volatile unsigned int *)&fake_io[(uintptr_t)(a) & 0x3FC])
#include "../src/sound.c"

static void wav(const char *path, const signed char *pcm, int n)
{
    FILE *f = fopen(path, "wb");
    unsigned int rate = 10512, sz = (unsigned)n;
    fwrite("RIFF", 1, 4, f); unsigned int t = 36 + sz; fwrite(&t, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    unsigned int fmt[4] = { 16, 1 | (1 << 16), rate, rate }; fwrite(fmt, 4, 4, f);
    unsigned short ba[2] = { 1, 8 }; fwrite(ba, 2, 2, f);
    fwrite("data", 1, 4, f); fwrite(&sz, 4, 1, f);
    for (int i = 0; i < n; i++) { unsigned char u = (unsigned char)(pcm[i] + 128); fwrite(&u, 1, 1, f); }
    fclose(f);
}

int main(int argc, char **argv)
{
    static const char *names[NSFX] = { "tune", "pickup", "glass", "start", "pass", "fail", "yelp", "cash", "ricochet", "meow" };
    const char *pre = argc > 1 ? argv[1] : "sfx_";
    static signed char pcm[SAMPLES * 200];
    for (int k = 0; k < NSFX; k++) {
        memset(&g_snd, 0, sizeof g_snd); memset(sv, 0, sizeof sv); memset(sfx_pend, 0, sizeof sfx_pend);
        g_snd.sfx = 1 << k;
        int peak = 0, frames = 0;
        for (int fr = 0; fr < 200; fr++) {
            sfx_tick();
            mix(pcm + fr * SAMPLES);
            int active = 0;
            for (int i = 0; i < SAMPLES; i++) { int v = pcm[fr * SAMPLES + i]; if (v < 0) v = -v; if (v > peak) peak = v; if (v > 2) active = 1; }
            if (active) frames = fr + 1;
        }
        char path[256]; snprintf(path, sizeof path, "%s%s.wav", pre, names[k]);
        wav(path, pcm, (frames + 6) * SAMPLES);
        printf("%-9s %5.2f s  peak %3d/127\n", names[k], frames / 59.73, peak);
    }
    return 0;
}
