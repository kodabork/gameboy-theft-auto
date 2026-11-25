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
