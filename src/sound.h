#ifndef SOUND_H
#define SOUND_H
#define NSTATIONS 5
typedef struct {
    volatile int engine_rpm, engine_vol;   /* vol 0..64 */
    volatile int horn, skid;               /* horn 0/1, skid 0..48 */
    volatile int crash, step, door;        /* one-shot triggers */
    volatile int radio_on;
    volatile int station;                  /* radio station 0..NSTATIONS-1 */
    volatile int nitro, hit, siren;        /* nitro whoosh on/off, body-hit thud trigger */
    volatile int sfx;                      /* one-shot effects: OR in SFX_* bits */
    volatile int shot, shot_vol;           /* shot: 1 pistol 2 SMG 3 shotgun 4 rifle 5 explosion 6 rocket launch; vol 0..64 (0 = full) */
} SoundState;
extern SoundState g_snd;
enum { SFX_TUNE = 1, SFX_PICKUP = 2, SFX_GLASS = 4, SFX_START = 8, SFX_PASS = 16,
       SFX_FAIL = 32, SFX_YELP = 64, SFX_CASH = 128, SFX_RICOCHET = 256, SFX_MEOW = 512,
       SFX_PUMP = 1024, SFX_BOLT = 2048, SFX_SHELL = 4096, SFX_LAUNCH = 8192, SFX_DRY = 16384, SFX_SWAP = 32768, NSFX = 16 };
void sound_init(void);
void sound_vblank(void);    /* called from the VBlank interrupt */
#endif
