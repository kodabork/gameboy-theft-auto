@ Hand-written ARM inner loops for the renderer (IWRAM). Each matches a C version in render.c
@ (used by the PC build) pixel for pixel.
    .section .iwram, "ax"
    .arm
    .align 2
    .global span_a, span_b, span_ab, span_mag

@ Sprite column pieces. p = frame buffer halfword, n rows (stride 240 bytes), v = source row 16.16,
@ inv = step per row, s = source column (index by v >> 16). Colour index + 103 (SPR_PAL_BASE);
@ 0 is transparent. VRAM takes only halfword stores, so single pixels are read-modify-write.

@ void span_a(u16 *p, int n, int v, int inv, const u8 *sa)    low (left) pixel only
span_a:
    cmp     r1, #0
    bxle    lr
    push    {r4-r6, lr}
    ldr     r4, [sp, #16]
1:  ldrb    r5, [r4, r2, asr #16]
    add     r2, r2, r3
    cmp     r5, #0
    beq     2f
    ldrh    r6, [r0]
    add     r5, r5, #103
    bic     r6, r6, #0xFF
    orr     r6, r6, r5
    strh    r6, [r0]
2:  add     r0, r0, #240
    subs    r1, r1, #1
    bne     1b
    pop     {r4-r6, lr}
    bx      lr

@ void span_b(u16 *p, int n, int v, int inv, const u8 *sb)    high (right) pixel only
span_b:
    cmp     r1, #0
    bxle    lr
    push    {r4-r6, lr}
    ldr     r4, [sp, #16]
1:  ldrb    r5, [r4, r2, asr #16]
    add     r2, r2, r3
    cmp     r5, #0
    beq     2f
    ldrb    r6, [r0]                @ keep the low pixel (a byte read is fine; only stores must be halfwords)
    add     r5, r5, #103
    orr     r6, r6, r5, lsl #8
    strh    r6, [r0]
2:  add     r0, r0, #240
    subs    r1, r1, #1
    bne     1b
    pop     {r4-r6, lr}
    bx      lr

@ void span_ab(u16 *p, int n, int v, int inv, const u8 *sa, const u8 *sb)   both pixels, two columns
span_ab:
    cmp     r1, #0
    bxle    lr
    push    {r4-r8, lr}
    ldr     r4, [sp, #24]
    ldr     r5, [sp, #28]
1:  ldrb    r6, [r4, r2, asr #16]
    ldrb    r7, [r5, r2, asr #16]
    add     r2, r2, r3
    cmp     r6, #0
    cmpne   r7, #0
    beq     2f
    add     r6, r6, #103            @ both opaque: one plain store
    add     r7, r7, #103
    orr     r6, r6, r7, lsl #8
    strh    r6, [r0], #240
    subs    r1, r1, #1
    bne     1b
    pop     {r4-r8, lr}
    bx      lr
2:  orrs    r8, r6, r7
    beq     4f                      @ both transparent
    cmp     r6, #0
    beq     3f
    ldrh    r8, [r0]                @ only the low pixel
    add     r6, r6, #103
    bic     r8, r8, #0xFF
    orr     r8, r8, r6
    strh    r8, [r0]
    b       4f
3:  ldrb    r8, [r0]                @ only the high pixel
    add     r7, r7, #103
    orr     r8, r8, r7, lsl #8
    strh    r8, [r0]
4:  add     r0, r0, #240
    subs    r1, r1, #1
    bne     1b
    pop     {r4-r8, lr}
    bx      lr

@ void span_mag(u16 *p, int n, int v, int inv, const u8 *s)   both pixels from one column (magnified)
span_mag:
    cmp     r1, #0
    bxle    lr
    push    {r4-r5, lr}
    ldr     r12, [sp, #12]
1:  ldrb    r4, [r12, r2, asr #16]
    add     r2, r2, r3
    cmp     r4, #0
    addne   r4, r4, #103
    orrne   r4, r4, r4, lsl #8
    strhne  r4, [r0]
    add     r0, r0, #240
    subs    r1, r1, #1
    bne     1b
    pop     {r4-r5, lr}
    bx      lr
