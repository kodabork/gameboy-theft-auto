#ifndef INTERIORS_H
#define INTERIORS_H
#include "logo.h"
/* per shop type (0 gun, 1 store, 2 pawn, 3 cars, 4 bowling, 5 pool) */
#define NROOMS 6
extern const Img *const room_img[NROOMS], *const front_img[NROOMS], *const clerk_img[NROOMS][2];
extern const s16 clerk_pos[NROOMS][2][2];
extern const s16 front_y[NROOMS];
#endif
