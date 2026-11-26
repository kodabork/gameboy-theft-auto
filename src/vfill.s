@ Fill one framebuffer column with a 2-colour vertical dither, 8 rows per pass.
@ Each strh writes one row and steps down a row (post-increment of 240 bytes).
@ void vfill(u16 *dst, int rows, u16 c0, u16 c1)   rows >= 1
    .section .iwram, "ax", %progbits
    .arm
    .align 2
    .global vfill
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
