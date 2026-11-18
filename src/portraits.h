#ifndef PORTRAITS_H
#define PORTRAITS_H
/* cutscene portraits (tools/make_portraits.py): 48x48, one per speaking character */
#define NFACES 6
extern const u16 face_pal[NFACES][16];
extern const u32 face_tiles[NFACES][36 * 8];
extern const char *const face_names[NFACES];
#ifdef HOST
#include "logo.h"
extern const Img img_faces[NFACES];
#endif
#endif
