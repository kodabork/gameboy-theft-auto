/* Steel Harbor sound.
   DirectSound A (8-bit PCM, 10512 Hz, DMA double-buffered per frame): engine,
   horn, tyre squeal, crashes, doors, footsteps - all synthesised.
   PSG channels 1-4: an original 4-bar radio loop (lead, chords, bass, drums). */
#include "engine.h"
#include "sound.h"

SoundState g_snd;

#ifndef HOST
#define REG16(a) (*(volatile u16 *)(a))
#define REG32(a) (*(volatile u32 *)(a))

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

/* ------------------------------------------------------------ music data */
/* 64 sixteenth-steps (4 bars), Am - F - C - G. 0 = rest/sustain. MIDI notes. */
static const u8 bass[64] = {
    45,0,0,45, 0,0,52,0, 45,0,0,45, 0,0,55,57,
    41,0,0,41, 0,0,48,0, 41,0,0,41, 0,0,48,50,
    48,0,0,48, 0,0,55,0, 48,0,0,48, 0,0,52,55,
    43,0,0,43, 0,0,50,0, 43,0,47,0, 50,0,52,0 };
static const u8 lead[64] = {
    69,0,72,0, 76,0,0,74, 0,72,0,69, 0,0,0,0,
    69,0,72,0, 77,0,0,76, 0,72,0,74, 0,0,0,0,
    76,0,0,79, 0,0,76,0, 74,0,72,0, 0,0,74,0,
    71,0,0,74, 0,0,79,0, 78,0,74,0, 71,0,0,0 };
static const u8 chord[4][3] = { {57,60,64}, {53,57,60}, {55,60,64}, {55,59,62} };
/* drums: 1 kick, 2 snare, 3 hat */
static const u8 drums[16] = { 1,0,3,0, 2,0,3,1, 0,1,3,0, 2,0,3,3 };
static u32 mus_acc; static int mus_step = -1, chord_tog;

static void psg_init(void)
{
    REG16(0x04000084) = 0x0080;          /* master on */
    REG16(0x04000080) = 0xFF77;          /* PSG L/R vol 7, all channels both sides */
    REG16(0x04000060) = 0x0008;          /* ch1 no sweep */
    /* ch3 wave: a soft saw for bass */
    REG16(0x04000070) = 0x0040;
    static const u32 wave[4] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
    for (int i = 0; i < 4; i++) REG32(0x04000090 + i * 4) = wave[i];
    REG16(0x04000070) = 0x0000;
}

static void music_tick(void)
{
    if (!g_snd.radio_on) {
        if (mus_step >= 0) {             /* silence PSG */
            REG16(0x04000062) = 0; REG16(0x04000068) = 0; REG16(0x04000070) = 0; REG16(0x04000078) = 0;
            REG16(0x04000064) = 0x8000; REG16(0x0400006C) = 0x8000; REG16(0x0400007C) = 0x8000;
            mus_step = -1;
        }
        return;
    }
    mus_acc += 6698;                     /* 92 BPM sixteenths at 59.73 Hz, 16.16 */
    if (mus_acc < 65536 && mus_step >= 0) return;
    mus_acc &= 0xFFFF;
    mus_step = (mus_step + 1) & 63;
    int s = mus_step;
    if (lead[s]) {
        REG16(0x04000062) = 0xA780 | 0x0040;           /* vol 10, decay, duty 25% */
        REG16(0x04000064) = 0x8000 | psg_sq[lead[s]];
    }
    if ((s & 7) == 2 || (s & 7) == 6) {                 /* off-beat chord stabs, arpeggiated */
        int n = chord[s >> 4][chord_tog] + 12;
        if (++chord_tog == 3) chord_tog = 0;
        REG16(0x04000068) = 0x6180 | 0x0080;            /* vol 6, fast decay, duty 50% */
        REG16(0x0400006C) = 0x8000 | psg_sq[n];
    }
    if (bass[s]) {
        REG16(0x04000070) = 0x0080;
        REG16(0x04000072) = 0x2000;                     /* 100% */
        REG16(0x04000074) = 0x8000 | psg_wave[bass[s] - 12];
    }
    int d = drums[s & 15];
    if (d == 1)      { REG16(0x04000078) = 0xB200; REG16(0x0400007C) = 0x8000 | 0x0071; }  /* kick */
    else if (d == 2) { REG16(0x04000078) = 0xA200; REG16(0x0400007C) = 0x8000 | 0x0051; }  /* snare */
    else if (d == 3) { REG16(0x04000078) = 0x5100; REG16(0x0400007C) = 0x8000 | 0x0010; }  /* hat */
}

void sound_init(void)
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
            ec -= ec >> kc; eb -= eb >> kb; ebo -= ebo >> kbo; et -= et >> kt;
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
    mix(mixbuf[cur]);
    music_tick();
}
#else
void sound_init(void) {}
void sound_vblank(void) {}
#endif
