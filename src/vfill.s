@ Fill one framebuffer column with a 2-colour vertical dither, 8 rows per pass.
@ Each strh writes one row and steps down a row (post-increment of 240 bytes).
@ void vfill(u16 *dst, int rows, u16 c0, u16 c1)   rows >= 1 (r0 ends one line past the last row)
    .section .iwram, "ax", %progbits
    .arm
    .align 2
    .global vfill, vfill2
@ u16 *vfill2(u16 *dst, int rows, u32 c)   rows >= 1: c0 = low half (first row), c1 = high half; returns dst + rows lines
vfill2:
    mov     r3, r2, lsr #16
vfill:
    subs    r1, r1, #8
    blt     2f
1:  strh    r2, [r0], #240
    strh    r3, [r0], #240
    strh    r2, [r0], #240
    strh    r3, [r0], #240
    strh    r2, [r0], #240
    strh    r3, [r0], #240
    strh    r2, [r0], #240
    strh    r3, [r0], #240
    subs    r1, r1, #8
    bge     1b
2:  adds    r1, r1, #8
    bxeq    lr
3:  strh    r2, [r0], #240
    subs    r1, r1, #1
    bxeq    lr
    strh    r3, [r0], #240
    subs    r1, r1, #1
    bne     3b
    bx      lr

@ Street-grid ground spans (the C versions are in render.c, used by the PC build).
@ One texel per column: b = tile_pal[((gv >> 9) & 0x3F80) | ((gu >> 16) & 127)], and for the lot codes
@ 0xFE/0xFF, b = lot_base[(gv >> 23) * 21 + (gu >> 23)] + (b & 1). A column pair is written as one word:
@ (b0 | b1 << 16) * 0x0101 + fog offsets.
@ void grid_span(u32 *dst, int n, int gu, int gv, int su, int sv, u32 pp)            n >= 1
@ void grid_span2(u32 *dst, int n, int gu, int gv, int su, int sv, u32 pp0, u32 pp1)  n >= 1 (rows y, y+1)
.macro GSAMPLE rd
    and     \rd, r9, r3, lsr #9
    mov     r12, r2, lsl #9
    orr     \rd, \rd, r12, lsr #25
    ldrb    \rd, [r8, \rd]
    cmp     \rd, #0xFE
    blhs    glot_\rd
    add     r2, r2, r4
    add     r3, r3, r5
.endm
    .global grid_span, grid_span2
grid_span2:
    stmfd   sp!, {r4-r11, lr}
    add     r1, r0, r1, lsl #2          @ end
    add     r12, sp, #36
    ldmia   r12, {r4-r7}                @ su, sv, pp0, pp1
    ldr     r8, =tile_pal
    mov     r9, #0x3F80
1:  GSAMPLE r10
    GSAMPLE r11
    orr     r10, r10, r11, lsl #16
    add     r10, r10, r10, lsl #8
    add     r11, r10, r7
    str     r11, [r0, #240]
    add     r10, r10, r6
    str     r10, [r0], #4
    cmp     r0, r1
    blo     1b
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
grid_span:
    stmfd   sp!, {r4-r11, lr}
    add     r1, r0, r1, lsl #2
    add     r12, sp, #36
    ldmia   r12, {r4-r6}                @ su, sv, pp
    ldr     r8, =tile_pal
    mov     r9, #0x3F80
1:  GSAMPLE r10
    GSAMPLE r11
    orr     r10, r10, r11, lsl #16
    add     r10, r10, r10, lsl #8
    add     r10, r10, r6
    str     r10, [r0], #4
    cmp     r0, r1
    blo     1b
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
@ lot surfaces (rare): rd = lot_base[(gv >> 23) * 21 + (gu >> 23)] + (rd & 1)
.macro GLOT rd
glot_\rd:
    and     \rd, \rd, #1
    stmfd   sp!, {r0}
    mov     r0, r3, asr #23
    add     r12, r0, r0, lsl #2
    add     r12, r0, r12, lsl #2
    add     r12, r12, r2, asr #23
    ldr     r0, =lot_base
    ldrb    r12, [r0, r12]
    add     \rd, \rd, r12
    ldmfd   sp!, {r0}
    bx      lr
.endm
    GLOT r10
    GLOT r11
    .pool

@ Off-grid ground runs (C versions gen_run / gen_run2 in render.c, used by the PC build; render.c
@ checks the constants below with static asserts). u, v: terrain-map units (cell = 1 << 20).
@ A pixel inside the current reuse square uses its colour: r8, r9 = u, v of the pixel that set it,
@ r10 = colour * 0x0101 << 16 | (square size >> 16). The square is a whole terrain cell (size 1 << 20)
@ for plain terrain and the open sea, half a cell (1 << 19: one cell and one block of road samples)
@ for road with no lane line through that block; r8 = ~u (no reuse) for street-grid texels and
@ pixels with a lane line nearby. gmiss works out a new colour.
@ void gen_run(u16 *p, u16 *e, u32 u, u32 v, u32 su8, u32 sv8, u32 pp)
@ void gen_run2(u16 *p, u16 *e, u32 u, u32 v, u32 su8, u32 sv8, u32 pp0, u32 pp1)   rows y and y+1
    .global gen_run, gen_run2
gen_run2:
    stmfd   sp!, {r4-r11, lr}
    add     r12, sp, #36
    ldmia   r12, {r4-r7}
    mvn     r8, r2
    mov     r9, r3
    mov     r10, #16
    cmp     r0, r1
    bhs     9f
1:  eor     r11, r2, r8
    eor     r12, r3, r9
    orr     r11, r11, r12
    cmp     r11, r10, lsl #16
    blhs    gmiss
    add     r11, r7, r10, lsr #16
    strh    r11, [r0, #240]
    add     r11, r6, r10, lsr #16
    strh    r11, [r0], #2
    add     r2, r2, r4
    add     r3, r3, r5
    mov     r6, r6, ror #16
    mov     r7, r7, ror #16
    cmp     r0, r1
    blo     1b
9:  ldmfd   sp!, {r4-r11, lr}
    bx      lr
gen_run:
    stmfd   sp!, {r4-r11, lr}
    add     r12, sp, #36
    ldmia   r12, {r4-r6}
    mvn     r8, r2
    mov     r9, r3
    mov     r10, #16
    cmp     r0, r1
    bhs     9f
1:  eor     r11, r2, r8
    eor     r12, r3, r9
    orr     r11, r11, r12
    cmp     r11, r10, lsl #16
    blhs    gmiss
    add     r11, r6, r10, lsr #16
    strh    r11, [r0], #2
    add     r2, r2, r4
    add     r3, r3, r5
    mov     r6, r6, ror #16
    cmp     r0, r1
    blo     1b
9:  ldmfd   sp!, {r4-r11, lr}
    bx      lr
@ r10 = palette base (in r10) * 0x0101 << 16 | size
.macro PACK size
    mov     r10, r10, lsl #16
    orr     r10, r10, r10, lsl #8
    orr     r10, r10, #\size
.endm
@ in: r2 u, r3 v (r0-r7 preserved); out: r10, r8, r9 as above
gmiss:
    stmfd   sp!, {lr}
    sub     r11, r2, #0x04000000        @ g = gu << 3 (street-grid texel coordinates)
    sub     r12, r3, #0x04000000
    ldr     lr, =0x50F00000             @ GRID_TX << 19
    cmp     r11, lr
    cmplo   r12, lr
    bhs     1f
    mov     lr, r12, lsr #19            @ street grid: tile_pal[((h >> 19) & 127) << 7 | ((g >> 19) & 127)]
    and     lr, lr, #127
    mov     r10, r11, lsr #19
    and     r10, r10, #127
    orr     lr, r10, lr, lsl #7
    ldr     r10, =tile_pal
    ldrb    r10, [r10, lr]
    cmp     r10, #0xFE
    bhs     5f
4:  PACK    16
    mvn     r8, r2                      @ grid texels are smaller than a cell: no reuse
    ldmfd   sp!, {pc}
5:  and     r10, r10, #1                @ lot surface: lot_base[(h >> 26) * 21 + (g >> 26)] + (b & 1)
    mov     lr, r12, lsr #26
    add     r12, lr, lr, lsl #2
    add     r12, lr, r12, lsl #2
    add     r12, r12, r11, lsr #26
    ldr     lr, =lot_base
    ldrb    lr, [lr, r12]
    add     r10, r10, lr
    b       4b
1:  mov     r8, r2
    mov     r9, r3
    ldr     lr, =0x58E00000             @ (TMAP_W - 2) << 20
    cmp     r2, lr
    cmplo   r3, lr
    bhs     3f
    mov     r11, r3, lsr #20            @ idx = (v >> 20) * TMAP_W + (u >> 20)
    ldr     r12, =1424
    mul     lr, r11, r12
    add     r11, lr, r2, lsr #20
    ldr     r12, =terrain_m
    ldrb    r12, [r12, r11]             @ m
    and     lr, r12, #127               @ material
    cmp     lr, #5                      @ TM_ROAD .. TM_ROADE get lane lines (when gen_lanes)
    blo     2f
    cmp     lr, #8                      @ TM_PATH
    beq     2f
    ldr     r10, =gen_lanes
    ldr     r10, [r10]
    cmp     r10, #0
    beq     2f
    @ lane lines (lane_px in render.c): the 2x2 road samples around the pixel, cached per sample
    @ block in lane_blk { rk, ok, zone, a0, da, b0, db }
    stmfd   sp!, {r4-r6}
    sub     r10, r2, #0x80000           @ su = u - (8 << 16)
    sub     lr, r3, #0x80000            @ sv
    mov     r4, lr, asr #20
    ldr     r5, =1424
    mul     r6, r4, r5
    add     r6, r6, r10, asr #20        @ rk
    ldr     r4, =lane_blk
    ldr     r5, [r4]
    cmp     r5, r6
    bne     6f
7:  ldr     r5, [r4, #4]                @ ok: every sample on a road
    cmp     r5, #0
    beq     8f
    ldr     r5, [r4, #8]                @ zone of the whole block, or -1
    cmp     r5, #0
    bge     10f
    mvn     r8, r2                      @ a lane line crosses this block: per pixel
    mov     r5, r10, lsr #16
    and     r5, r5, #15                 @ fx
    ldr     r6, [r4, #16]
    mul     r6, r5, r6
    ldr     r10, [r4, #12]
    add     r10, r10, r6                @ la = a0 + fx * da
    ldr     r6, [r4, #24]
    mul     r6, r5, r6
    ldr     r5, [r4, #20]
    add     r6, r6, r5                  @ lb = b0 + fx * db
    sub     r6, r6, r10
    mov     r5, lr, lsr #16
    and     r5, r5, #15                 @ fy
    mul     r6, r5, r6
    add     r6, r6, r10, lsl #4
    mov     r6, r6, asr #5              @ t
    ldr     r5, =zone_of_t
    ldrb    r5, [r5, r6]
10: cmp     r5, #0
    cmpne   r5, #8
    beq     12f
    cmp     r5, #4
    bne     11f
    ldr     r6, =terrain_r
    ldrb    r6, [r6, r11]
    tst     r6, #0x80
    movne   r10, #25                    @ dashed centre line: 8 + MAT_YELLOW * 8 + 1
    bne     13f
11: ldr     r6, =zone_base
    ldrb    r10, [r6, r5]
    b       13f
12: mov     r10, #56                    @ concrete: 8 + MAT_CONCRETE * 8 + lit
    add     r10, r10, r12, lsr #7
    b       13f
8:  ldr     r6, =tm_b8                  @ not all samples on a road: plain terrain
    and     r5, r12, #127
    ldrb    r10, [r6, r5]
    add     r10, r10, r12, lsr #7
13: ldmfd   sp!, {r4-r6}
    PACK    8                           @ reuse over the half-cell (when r8 = u)
    ldmfd   sp!, {pc}
6:  stmfd   sp!, {r0-r3, r12, lr}       @ a new sample block (lr holds sv)
    mov     r0, r6
    bl      lane_block
    ldmfd   sp!, {r0-r3, r12, lr}
    b       7b
2:  ldr     r10, =tm_b8                 @ plain terrain: tm_b8[material] + lit
    ldrb    r10, [r10, lr]
    add     r10, r10, r12, lsr #7
    PACK    16
    ldmfd   sp!, {pc}
3:  eor     r10, r2, r3                 @ open sea beyond the map: 2 m checker
    mov     r10, r10, lsr #21
    and     r10, r10, #1
    add     r10, r10, #80               @ 8 + MAT_WATER * 8
    PACK    16
    ldmfd   sp!, {pc}
    .pool
