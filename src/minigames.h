#ifndef MINIGAMES_H
#define MINIGAMES_H
#include "engine.h"
enum { MINI_NONE, MINI_BOWL, MINI_POOL };
void mini_start(int kind);
int mini_update(u16 keys, u16 pressed, int dt);    /* 0 still playing, 1 the player won, 2 lost */
void mini_draw(void);
void mini_resume(void);                             /* after a pause: ignore A/B until released */
#endif
