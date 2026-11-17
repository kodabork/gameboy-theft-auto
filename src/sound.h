#ifndef SOUND_H
#define SOUND_H
typedef struct {
    volatile int engine_rpm, engine_vol;   /* vol 0..64 */
    volatile int horn, skid;               /* horn 0/1, skid 0..48 */
    volatile int crash, step, door;        /* one-shot triggers */
    volatile int radio_on;
    volatile int nitro, hit, siren;        /* nitro whoosh on/off, body-hit thud trigger */
    volatile int shot, shot_vol;           /* shot: 1 pistol 2 SMG 3 shotgun 4 rifle; vol 0..64 (0 = full) */
} SoundState;
extern SoundState g_snd;
void sound_init(void);
void sound_vblank(void);    /* called from the VBlank interrupt */
#endif
