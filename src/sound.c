/* Steel Harbor sound.
   DirectSound A (8-bit PCM, 10512 Hz, DMA double-buffered per frame): engine,
   horn, tyre squeal, crashes, doors, footsteps - all synthesised.
   PSG channels 1-4: an original 4-bar radio loop (lead, chords, bass, drums). */
#include "engine.h"
#include "sound.h"

SoundState g_snd;

#ifndef HOST
#ifndef REG16
#define REG16(a) (*(volatile u16 *)(a))
#endif
#ifndef REG32
#define REG32(a) (*(volatile u32 *)(a))
#endif

#define SAMPLES 176
EWRAM_BSS static s8 mixbuf[2][SAMPLES] __attribute__((aligned(4)));
static int cur;
EWRAM_BSS static s8 engine_wave[256];
static u32 eng_phase, horn_phase1, horn_phase2, skid_phase, rng = 0x9E3779B9;
static int crash_env, crash_lp, step_env, step_lp, door_env, hit_env, nitro_lp, hit_ph, siren_vol;
static u32 siren_ph, siren_lfo;
static int eng_vol_s;

static inline int noise(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (int)(rng & 255) - 128; }

static void build_engine_wave(void)
{
    /* two firing pulses per period with decaying ringing + a bit of roughness */
    for (int i = 0; i < 256; i++) {
        int t = i & 127;
        int env = 127 - t;                                    /* decays across each firing */
        int v = (sin_tab[(t * 96) & 4095] * env) >> 14;       /* ring */
        v += (sin_tab[(i * 16) & 4095] * 30) >> 14;           /* fundamental */
        v += (sin_tab[(i * 48) & 4095] * 18) >> 14;
        if (i >= 128) v = v * 7 / 8;                           /* uneven cylinders */
        v += (noise() * 12) >> 7;
        if (v > 127) v = 127; if (v < -127) v = -127;
        engine_wave[i] = (s8)v;
    }
}

/* ------------------------------------------------------------ radio stations */
/* Five original 4-bar loops on the PSG: lead (ch1), chord stabs (ch2), bass (ch3 wave), drums
   (ch4 noise). Each pattern is 64 sixteenth steps, MIDI note numbers, 0 = rest / sustain.
   Drums: 1 kick, 2 snare, 3 closed hat, 4 open hat (32 steps). */
typedef struct {
    u16 step;                 /* sixteenths per VBlank, 16.16 (BPM * 73.15) */
    u8 stab_mask;             /* chord stab when bit (s & 7) is set */
    u8 wave;                  /* bass wavetable */
    const u8 *bass, *lead, *chords, *drums;
    u16 lead_nr, stab_nr;     /* PSG envelope + duty for lead and stabs */
} Station;

/* HARBOR FM: synth-pop, 92 BPM, Am - F - C - G */
static const u8 hb_bass[64] = {
    45,0,0,45, 0,0,52,0, 45,0,0,45, 0,0,55,57,  41,0,0,41, 0,0,48,0, 41,0,0,41, 0,0,48,50,
    48,0,0,48, 0,0,55,0, 48,0,0,48, 0,0,52,55,  43,0,0,43, 0,0,50,0, 43,0,47,0, 50,0,52,0 };
static const u8 hb_lead[64] = {
    69,0,72,0, 76,0,0,74, 0,72,0,69, 0,0,0,0,  69,0,72,0, 77,0,0,76, 0,72,0,74, 0,0,0,0,
    76,0,0,79, 0,0,76,0, 74,0,72,0, 0,0,74,0,  71,0,0,74, 0,0,79,0, 78,0,74,0, 71,0,0,0 };
static const u8 hb_chords[12] = { 57,60,64, 53,57,60, 55,60,64, 55,59,62 };
static const u8 hb_drums[32] = { 1,0,3,0, 2,0,3,1, 0,1,3,0, 2,0,3,3,  1,0,3,0, 2,0,3,1, 0,1,3,0, 2,0,3,3 };

/* KICK 104.5: club house, 124 BPM, Fm - Db - Ab - Eb */
static const u8 kk_bass[64] = {
    41,0,53,0, 0,0,53,0, 41,0,53,0, 0,41,53,0,  37,0,49,0, 0,0,49,0, 37,0,49,0, 0,37,49,0,
    44,0,56,0, 0,0,56,0, 44,0,56,0, 0,44,56,0,  39,0,51,0, 0,0,51,0, 39,0,51,0, 0,39,51,53 };
static const u8 kk_lead[64] = {
    72,0,68,0, 65,0,68,0, 72,0,75,0, 72,0,68,0,  73,0,68,0, 65,0,68,0, 73,0,77,0, 73,0,68,0,
    72,0,68,0, 63,0,68,0, 72,0,75,0, 80,0,75,0,  70,0,67,0, 63,0,67,0, 70,0,75,0, 74,0,70,0 };
static const u8 kk_chords[12] = { 53,56,60, 49,53,56, 56,60,63, 51,55,58 };
static const u8 kk_drums[32] = { 1,0,4,0, 2,0,4,0, 1,0,4,0, 2,0,4,3,  1,0,4,0, 2,0,4,0, 1,0,4,1, 2,0,2,4 };

/* IRON 97: guitar rock, 140 BPM, Em - C - G - D */
static const u8 ir_bass[64] = {
    40,0,40,0, 40,0,40,0, 40,0,40,0, 52,0,47,0,  36,0,36,0, 36,0,36,0, 36,0,36,0, 48,0,43,0,
    43,0,43,0, 43,0,43,0, 43,0,43,0, 55,0,50,0,  38,0,38,0, 38,0,38,0, 38,0,38,0, 45,0,47,0 };
static const u8 ir_lead[64] = {
    64,0,0,67, 0,0,69,0, 71,0,69,0, 67,0,64,0,  64,0,0,67, 0,0,72,0, 71,0,69,0, 67,0,0,0,
    67,0,0,71, 0,0,74,0, 72,0,71,0, 67,0,71,0,  69,0,0,66, 0,0,62,0, 66,0,69,0, 71,0,0,0 };
static const u8 ir_chords[12] = { 52,59,64, 48,55,60, 55,62,67, 50,57,62 };
static const u8 ir_drums[32] = { 1,0,3,0, 2,0,3,0, 1,0,1,0, 2,0,3,3,  1,0,3,0, 2,0,3,0, 1,0,1,0, 2,2,2,2 };

/* LOW END 88: laid-back hip-hop, 88 BPM, Dm - Bb - Gm - A */
static const u8 le_bass[64] = {
    38,0,0,0, 0,0,0,38, 0,0,50,0, 0,0,45,0,  46,0,0,0, 0,0,0,46, 0,0,41,0, 0,0,46,0,
    43,0,0,0, 0,0,0,43, 0,0,50,0, 0,0,46,0,  45,0,0,0, 0,0,0,45, 0,0,49,0, 52,0,45,0 };
static const u8 le_lead[64] = {
    74,0,0,0, 0,0,77,0, 0,0,76,0, 74,0,0,0,  72,0,0,0, 0,0,74,0, 0,0,70,0, 0,0,0,0,
    70,0,0,0, 0,0,74,0, 0,0,72,0, 70,0,67,0,  69,0,0,0, 73,0,0,0, 76,0,0,0, 0,0,0,0 };
static const u8 le_chords[12] = { 50,53,57, 46,50,53, 43,46,50, 45,49,52 };
static const u8 le_drums[32] = { 1,0,3,0, 2,0,3,1, 0,1,3,0, 2,0,3,0,  1,0,3,0, 2,0,3,0, 1,0,3,1, 2,0,3,3 };

/* SMOOTH 101: late-night jazz, 100 BPM, Dm7 - G7 - Cmaj7 - Am7, walking bass */
static const u8 sm_bass[64] = {
    38,0,0,0, 41,0,0,0, 45,0,0,0, 44,0,0,0,  43,0,0,0, 47,0,0,0, 50,0,0,0, 49,0,0,0,
    48,0,0,0, 47,0,0,0, 45,0,0,0, 43,0,0,0,  45,0,0,0, 43,0,0,0, 41,0,0,0, 40,0,0,0 };
static const u8 sm_lead[64] = {
    74,0,0,72, 0,0,69,0, 0,0,72,0, 74,0,0,0,  77,0,0,76, 0,0,74,0, 71,0,0,0, 67,0,0,0,
    76,0,0,0, 0,0,74,0, 72,0,71,0, 72,0,0,0,  69,0,0,72, 0,0,76,0, 0,0,79,0, 77,0,76,0 };
static const u8 sm_chords[12] = { 53,57,60, 53,55,59, 52,55,59, 55,57,60 };
static const u8 sm_drums[32] = { 1,0,0,0, 3,0,0,3, 3,0,0,0, 3,0,2,3,  1,0,0,0, 3,0,0,3, 3,0,1,0, 3,0,2,3 };

static const Station stations[NSTATIONS] = {
    { 6730, 0x44, 0, hb_bass, hb_lead, hb_chords, hb_drums, 0xA780 | 0x0040, 0x6180 | 0x0080 },
    { 9071, 0x88, 1, kk_bass, kk_lead, kk_chords, kk_drums, 0x8180 | 0x0080, 0x5180 | 0x0040 },
    { 10241, 0x11, 1, ir_bass, ir_lead, ir_chords, ir_drums, 0xB780 | 0x0080, 0x7380 | 0x00C0 },
    { 6437, 0x10, 0, le_bass, le_lead, le_chords, le_drums, 0x9600 | 0x0000, 0x5280 | 0x0080 },
    { 7315, 0x48, 2, sm_bass, sm_lead, sm_chords, sm_drums, 0x8500 | 0x0080, 0x4380 | 0x0080 },
};
/* bass wavetables: soft saw, square-ish growl, round sine-like */
static const u32 bass_waves[3][4] = {
    { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 },
    { 0xFFDFBF9F, 0xFFFFFFFF, 0x20406080, 0x00000000 },
    { 0xCBA98877, 0xFFFEEDDC, 0x34567788, 0x00011223 },
};
static u32 mus_acc; static int mus_step = -1, chord_tog, cur_station = -1;

static void load_wave(int w)
{
    REG16(0x04000070) = 0x0040;          /* write the bank that is not playing */
    for (int i = 0; i < 4; i++) REG32(0x04000090 + i * 4) = bass_waves[w][i];
    REG16(0x04000070) = 0x0000;
}

static void psg_init(void)
{
    REG16(0x04000084) = 0x0080;          /* master on */
    REG16(0x04000080) = 0xFF77;          /* PSG L/R vol 7, all channels both sides */
    REG16(0x04000060) = 0x0008;          /* ch1 no sweep */
    load_wave(0);
}

static void psg_silence(void)
{
    REG16(0x04000062) = 0; REG16(0x04000068) = 0; REG16(0x04000070) = 0; REG16(0x04000078) = 0;
    REG16(0x04000064) = 0x8000; REG16(0x0400006C) = 0x8000; REG16(0x0400007C) = 0x8000;
}

/* runs once per VBlank: tiny, so it lives in ROM to save IWRAM */
__attribute__((section(".romcode"))) static void music_tick(void)
{
    if (!g_snd.radio_on) {
        if (mus_step >= 0) { psg_silence(); mus_step = -1; }
        return;
    }
    int st = g_snd.station; if ((unsigned)st >= NSTATIONS) st = 0;
    if (st != cur_station) {             /* retune: new wavetable, start the loop over */
        psg_silence(); load_wave(stations[st].wave);
        cur_station = st; mus_step = -1; mus_acc = 0; chord_tog = 0;
    }
    const Station *S = &stations[st];
    mus_acc += S->step;
    if (mus_acc < 65536 && mus_step >= 0) return;
    mus_acc &= 0xFFFF;
    mus_step = (mus_step + 1) & 63;
    int s = mus_step;
    if (S->lead[s]) {
        REG16(0x04000062) = S->lead_nr;
        REG16(0x04000064) = 0x8000 | psg_sq[S->lead[s]];
    }
    if ((S->stab_mask >> (s & 7)) & 1) {                /* chord stabs, arpeggiated */
        int n = S->chords[(s >> 4) * 3 + chord_tog] + 12;
        if (++chord_tog == 3) chord_tog = 0;
        REG16(0x04000068) = S->stab_nr;
        REG16(0x0400006C) = 0x8000 | psg_sq[n];
    }
    if (S->bass[s]) {
        REG16(0x04000070) = 0x0080;
        REG16(0x04000072) = 0x2000;                     /* 100% */
        REG16(0x04000074) = 0x8000 | psg_wave[S->bass[s] - 12];
    }
    int d = S->drums[s & 31];
    if (d == 1)      { REG16(0x04000078) = 0xB200; REG16(0x0400007C) = 0x8000 | 0x0071; }  /* kick */
    else if (d == 2) { REG16(0x04000078) = 0xA200; REG16(0x0400007C) = 0x8000 | 0x0051; }  /* snare */
    else if (d == 3) { REG16(0x04000078) = 0x5100; REG16(0x0400007C) = 0x8000 | 0x0010; }  /* closed hat */
    else if (d == 4) { REG16(0x04000078) = 0x6300; REG16(0x0400007C) = 0x8000 | 0x0011; }  /* open hat */
}

__attribute__((section(".romcode"))) void sound_init(void)   /* runs once: ROM, not IWRAM */
{
    psg_init();
    build_engine_wave();
    REG16(0x04000082) = 0x0B06;          /* PSG 100%, DSA 100% to L+R, timer 0, reset FIFO */
    REG16(0x04000100) = (u16)(65536 - 1596);   /* timer 0: 10512 Hz */
    REG16(0x04000102) = 0x0080;
    REG32(0x040000C0) = 0x040000A0;      /* DMA1 dest = FIFO A */
    g_snd.radio_on = 1;
}

/* ------------------------------------------------------------ gunfire voices */
/* Each shot = sharp crack (bright noise) + body (filtered noise) + boom (sine with a falling
   pitch) + room tail (dark noise, slow decay). Per-weapon recipes; two voices so the player's
   and the cops' shots can overlap. Envelopes are 8.8 fixed point, decaying by e -= e >> k. */
typedef struct { u8 crack, kc, body, kb, lpb, boom, kbo, tail, kt, lpt; u16 f0, f1; } GunRecipe;
static const GunRecipe recipes[6] = {
    { 0 },
    {  96, 5,  64, 8, 1,  72,  9, 20, 10, 3, 150, 55 },    /* pistol */
    {  80, 5,  46, 7, 1,  44,  8, 12,  9, 3, 120, 70 },    /* SMG */
    { 104, 6,  92, 9, 2,  96, 10, 30, 11, 4,  95, 38 },    /* shotgun */
    { 124, 4,  70, 8, 1,  64,  9, 26, 11, 4, 180, 60 },    /* rifle */
    { 110, 6, 120, 10, 2, 127, 11, 60, 12, 5,  70, 24 },   /* explosion */
};
typedef struct { int ec, eb, ebo, et, lb, lt; u32 ph, f, f1; u8 kc, kb, kbo, kt, lpb, lpt; } GunVoice;
static GunVoice gv[2]; static int gv_next;

static void gun_trigger(int type, int vol)
{
    const GunRecipe *r = &recipes[type];
    GunVoice *v = &gv[gv_next]; gv_next ^= 1;
    v->ec = r->crack * vol << 2; v->eb = r->body * vol << 2; v->ebo = r->boom * vol << 2; v->et = r->tail * vol << 2;
    v->kc = r->kc; v->kb = r->kb; v->kbo = r->kbo; v->kt = r->kt; v->lpb = r->lpb; v->lpt = r->lpt;
    v->f = (u32)r->f0 * 408568u; v->f1 = (u32)r->f1 * 408568u; v->ph = 0;   /* Hz -> 32-bit phase step */
}

static s16 acc[SAMPLES];
/* ------------------------------------------------------------ effect synth */
/* Each effect is a few notes: waveform, start -> end pitch (exponential glide), volume and an
   exponential decay (time constant 2^decay samples). Notes can start a few frames apart. */
enum { W_SINE, W_SQUARE, W_NOISE_LO, W_NOISE_HI, W_TRI };
typedef struct { u8 delay, wave; u16 f0, f1; u8 vol, decay, glide; } SfxNote;   /* vol 0 = end */
static const SfxNote sfx_tune[] = { {0, W_NOISE_HI, 0, 0, 46, 11, 0}, {0, W_SINE, 2600, 300, 22, 11, 9}, {0} };
static const SfxNote sfx_pickup[] = { {0, W_SINE, 1319, 1319, 70, 10, 0}, {4, W_SINE, 1976, 1976, 70, 11, 0}, {4, W_SQUARE, 988, 988, 14, 10, 0}, {0} };
static const SfxNote sfx_glass[] = { {0, W_NOISE_HI, 0, 0, 92, 10, 0}, {2, W_SINE, 4200, 4100, 42, 8, 12}, {4, W_SINE, 5300, 5200, 36, 8, 12},
                                     {7, W_SINE, 3700, 3650, 34, 8, 12}, {10, W_SINE, 6100, 6000, 30, 8, 12}, {0} };
static const SfxNote sfx_start[] = { {0, W_NOISE_LO, 0, 0, 70, 9, 0}, {5, W_NOISE_LO, 0, 0, 66, 9, 0}, {10, W_NOISE_LO, 0, 0, 62, 9, 0},
                                     {15, W_NOISE_LO, 0, 0, 60, 9, 0}, {20, W_SQUARE, 45, 95, 44, 12, 12}, {0} };
/* cat: a short rising-then-falling mew */
static const SfxNote sfx_meow[] = { {0, W_SQUARE, 620, 1150, 26, 11, 9}, {6, W_SQUARE, 1150, 560, 30, 11, 10}, {6, W_SINE, 1700, 900, 18, 11, 10}, {0} };
static const SfxNote sfx_pass[] = { {0, W_TRI, 523, 523, 70, 12, 0}, {6, W_TRI, 659, 659, 70, 12, 0}, {12, W_TRI, 784, 784, 70, 12, 0},
                                    {18, W_TRI, 1047, 1047, 80, 13, 0}, {18, W_SQUARE, 523, 523, 22, 13, 0}, {0} };
static const SfxNote sfx_fail[] = { {0, W_SQUARE, 392, 392, 40, 12, 0}, {9, W_SQUARE, 370, 370, 40, 12, 0}, {18, W_SQUARE, 349, 349, 40, 12, 0},
                                    {27, W_SQUARE, 330, 300, 44, 13, 13}, {0} };
static const SfxNote sfx_yelp[] = { {0, W_SQUARE, 950, 380, 34, 11, 10}, {0, W_SINE, 1250, 520, 30, 11, 10}, {0} };
static const SfxNote sfx_cash[] = { {0, W_SQUARE, 1568, 1568, 30, 9, 0}, {3, W_SQUARE, 2093, 2093, 30, 9, 0}, {6, W_SINE, 2637, 2637, 60, 11, 0}, {0} };
static const SfxNote sfx_ricochet[] = { {0, W_SINE, 3400, 900, 44, 11, 11}, {0, W_NOISE_HI, 0, 0, 30, 8, 0}, {0} };
static const SfxNote *const sfx_list[NSFX] = { sfx_tune, sfx_pickup, sfx_glass, sfx_start, sfx_pass,
                                               sfx_fail, sfx_yelp, sfx_cash, sfx_ricochet, sfx_meow };
typedef struct { u32 ph, f, f1; int env, lp; u8 wave, decay, glide; } SfxVoice;
#define NSV 4
static SfxVoice sv[NSV]; static int sv_next;
typedef struct { const SfxNote *n; u8 wait; } SfxPend;
EWRAM_BSS static SfxPend sfx_pend[8];

/* once per VBlank: start effects that were asked for, and notes whose delay has come */
__attribute__((section(".romcode"))) static void sfx_tick(void)
{
    int m = g_snd.sfx;
    if (m) {
        g_snd.sfx = 0;
        for (int k = 0; k < NSFX; k++) if ((m >> k) & 1)
            for (int j = 0; j < 8; j++) if (!sfx_pend[j].n) { sfx_pend[j].n = sfx_list[k]; sfx_pend[j].wait = 0; break; }
    }
    for (int j = 0; j < 8; j++) {
        SfxPend *p = &sfx_pend[j];
        if (!p->n) continue;
        int t = p->wait++;
        while (p->n->vol && p->n->delay <= t) {
            const SfxNote *n = p->n++;
            SfxVoice *v = &sv[sv_next]; sv_next = (sv_next + 1) & (NSV - 1);
            v->ph = 0; v->f = (u32)n->f0 * 408568u; v->f1 = (u32)n->f1 * 408568u;   /* Hz -> phase step */
            v->env = n->vol << 8; v->lp = 0; v->wave = n->wave; v->decay = n->decay; v->glide = n->glide;
        }
        if (!p->n->vol) p->n = 0;
    }
}


static void mix(s8 *out)
{
    int ev = g_snd.engine_vol;
    eng_vol_s += (ev - eng_vol_s) / 4;
    u32 estep = (u32)g_snd.engine_rpm * 27;               /* rpm/60 Hz over 256-sample wave */
    int horn = g_snd.horn, skid = g_snd.skid;
    if (g_snd.crash) { crash_env = g_snd.crash * 256; g_snd.crash = 0; }
    if (g_snd.step) { step_env = 40 * 256; g_snd.step = 0; }
    if (g_snd.door) { door_env = 60 * 256; g_snd.door = 0; }
    if (g_snd.hit) { hit_env = 64 * 256; g_snd.hit = 0; hit_ph = 0; }
    int nit = g_snd.nitro;
    if (g_snd.shot) { int vol = g_snd.shot_vol ? g_snd.shot_vol : 64; gun_trigger(g_snd.shot > 5 ? 1 : g_snd.shot, vol); g_snd.shot = 0; g_snd.shot_vol = 0; }
    siren_vol += (g_snd.siren - siren_vol) / 4;
    int i;

    /* engine (always present as the bed of the mix) */
    if (eng_vol_s) {
        u32 ph = eng_phase; int ve = eng_vol_s;
        for (i = 0; i < SAMPLES; i++) { ph += estep; acc[i] = (s16)((engine_wave[(ph >> 16) & 255] * ve) >> 6); }
        eng_phase = ph;
    } else for (i = 0; i < SAMPLES; i++) acc[i] = 0;

    if (horn) {                                            /* two-tone horn ~ 410 / 520 Hz */
        for (i = 0; i < SAMPLES; i++) {
            horn_phase1 += 167500000u; horn_phase2 += 212450000u;
            acc[i] += ((horn_phase1 >> 31) ? 22 : -22) + ((horn_phase2 >> 31) ? 18 : -18);
        }
    }
    if (skid) {
        for (i = 0; i < SAMPLES; i++) {
            skid_phase += 520000000u + (u32)(noise() << 18);
            acc[i] += (sin_tab[skid_phase >> 20] * skid) >> 15;
        }
    }
    if (crash_env > 0) {
        for (i = 0; i < SAMPLES && crash_env > 0; i++) {
            crash_lp += (noise() - crash_lp) >> 1;
            acc[i] += (crash_lp * (crash_env >> 8)) >> 6;
            crash_env -= 5;
        }
    }
    if (step_env > 0) {
        for (i = 0; i < SAMPLES && step_env > 0; i++) {
            step_lp += (noise() - step_lp) >> 2;
            acc[i] += (step_lp * (step_env >> 8)) >> 6;
            step_env -= 60;
        }
    }
    for (int k = 0; k < 2; k++) {                           /* gunfire */
        GunVoice *v = &gv[k];
        if ((v->ec | v->eb | v->ebo | v->et) < 256) continue;
        int ec = v->ec, eb = v->eb, ebo = v->ebo, et = v->et, lb = v->lb, lt = v->lt;
        u32 ph = v->ph, f = v->f; const u32 f1 = v->f1;
        const int kc = v->kc, kb = v->kb, kbo = v->kbo, kt = v->kt, lpb = v->lpb, lpt = v->lpt;
        for (i = 0; i < SAMPLES; i++) {
            int n = noise();
            lb += (n - lb) >> lpb; lt += (n - lt) >> lpt;
            ph += f; f -= (f - f1) >> 7;
            int s = ((n * (ec >> 8)) >> 7) + ((lb * (eb >> 8)) >> 6) + ((sin_tab[ph >> 20] * (ebo >> 8)) >> 14) + ((lt * (et >> 8)) >> 5);
            acc[i] += s;
            ec -= (ec >> kc) + (ec > 0); eb -= (eb >> kb) + (eb > 0); ebo -= (ebo >> kbo) + (ebo > 0); et -= (et >> kt) + (et > 0);   /* +1: fades really end */
        }
        v->ec = ec; v->eb = eb; v->ebo = ebo; v->et = et; v->lb = lb; v->lt = lt; v->ph = ph; v->f = f;
    }
    if (siren_vol > 0) {                                   /* two-tone wail */
        for (i = 0; i < SAMPLES; i++) {
            siren_lfo += 7;
            int f = 650 + ((sin_tab[(siren_lfo >> 4) & 4095] + 16384) * 600 >> 15);
            siren_ph += (u32)f * 408568u;
            acc[i] += (sin_tab[siren_ph >> 20] * siren_vol) >> 15;
        }
    }
    if (nit) {                                             /* nitro: rushing hiss */
        for (i = 0; i < SAMPLES; i++) { nitro_lp += (noise() - nitro_lp) >> 1; acc[i] += (nitro_lp * 22) >> 6; }
    }
    if (hit_env > 0) {                                     /* body hit: low thump */
        for (i = 0; i < SAMPLES && hit_env > 0; i++) {
            hit_ph += 1 + (hit_env >> 12);
            acc[i] += (sin_tab[(hit_ph * 40) & 4095] * (hit_env >> 8)) >> 15;
            hit_env -= 14;
        }
    }
    if (door_env > 0) {
        for (i = 0; i < SAMPLES && door_env > 0; i++) {
            int t = 60 - (door_env >> 8);
            acc[i] += ((t < 6 ? noise() : (noise() >> 3)) * (door_env >> 8)) >> 6;
            door_env -= 40;
        }
    }
    for (int k = 0; k < NSV; k++) {                        /* effect voices */
        SfxVoice *v = &sv[k];
        if (v->env < 256) continue;
        u32 ph = v->ph, f = v->f; const u32 f1 = v->f1; int env = v->env, lp = v->lp;
        const int w = v->wave, kd = v->decay, kg = v->glide;
        for (i = 0; i < SAMPLES && env >= 256; i++) {
            int x;
            ph += f;
            if (kg) f += (u32)((int)(f1 - f) >> kg);
            if (w == W_SINE) x = sin_tab[ph >> 20] >> 7;
            else if (w == W_SQUARE) x = (ph >> 31) ? 100 : -100;
            else if (w == W_TRI) { int t = (int)(ph >> 23); x = (t < 256 ? t : 511 - t) - 128; }
            else if (w == W_NOISE_LO) { lp += (noise() - lp) >> 2; x = lp * 2; }
            else x = noise();
            acc[i] += (x * (env >> 8)) >> 7;
            env -= (env >> kd) | 1;                        /* | 1: always reaches silence */
        }
        if (i < SAMPLES) env = 0;
        v->ph = ph; v->f = f; v->env = env; v->lp = lp;
    }
    for (i = 0; i < SAMPLES; i++) {                        /* soft-ish clip */
        int v = acc[i];
        if (v > 96) v = 96 + ((v - 96) >> 2);
        else if (v < -96) v = -96 + ((v + 96) >> 2);
        if (v > 127) v = 127; else if (v < -128) v = -128;
        out[i] = (s8)v;
    }
}

void sound_vblank(void)
{
    /* restart DMA on the buffer mixed last frame, then mix the next one */
    REG16(0x040000C6) = 0;
    REG32(0x040000BC) = (u32)mixbuf[cur];
    REG16(0x040000C6) = 0xB640;
    cur ^= 1;
    sfx_tick();
    mix(mixbuf[cur]);
    music_tick();
}
#else
void sound_init(void) {}
void sound_vblank(void) {}
#endif
