@ Integer division helpers + VBlank interrupt handler. ARM code in IWRAM.
    .section .iwram, "ax"
    .arm
    .align 2
    .global __aeabi_idiv, __aeabi_idivmod, __aeabi_uidiv, __aeabi_uidivmod

@ Shift-subtract division, four quotient bits per pass of the main loop.
@ r0 = numerator, r1 = denominator  ->  r0 = quotient, r1 = remainder
@ The divisor is first shifted up (by 3 when there is room, then by 4s, then by 1s) until it passes
@ the numerator, with r3 holding the matching quotient bit. Each pass then tries the divisor at
@ shifts 0..3 below that. When the divisor could not take the first shift of 3, the last pass also
@ tries shifts below quotient bit 0; those subtractions add nothing to the quotient (their bits are
@ zero) but spoil the running remainder, so the remainder is worked out at the end as n - q * d.
__aeabi_uidiv:
__aeabi_uidivmod:
    cmp     r1, #0
    moveq   r0, #0
    bxeq    lr
    stmfd   sp!, {r0, r1}               @ keep n and d for the remainder
    mov     r2, r1                      @ shifted divisor
    tst     r2, #0xE0000000
    moveq   r2, r2, lsl #3
    moveq   r3, #8
    movne   r3, #1
1:  cmp     r2, #0x10000000             @ coarse: 4 bits at a time while it stays below n and bit 28
    cmplo   r2, r0
    movlo   r2, r2, lsl #4
    movlo   r3, r3, lsl #4
    blo     1b
2:  cmp     r2, #0x80000000             @ fine: 1 bit at a time
    cmplo   r2, r0
    movlo   r2, r2, lsl #1
    movlo   r3, r3, lsl #1
    blo     2b
    mov     r12, #0
3:  cmp     r0, r2
    subhs   r0, r0, r2
    orrhs   r12, r12, r3
    cmp     r0, r2, lsr #1
    subhs   r0, r0, r2, lsr #1
    orrhs   r12, r12, r3, lsr #1
    cmp     r0, r2, lsr #2
    subhs   r0, r0, r2, lsr #2
    orrhs   r12, r12, r3, lsr #2
    cmp     r0, r2, lsr #3
    subhs   r0, r0, r2, lsr #3
    orrhs   r12, r12, r3, lsr #3
    cmp     r0, #0                      @ nothing left to divide: the remaining bits are 0
    movsne  r3, r3, lsr #4
    movne   r2, r2, lsr #4
    bne     3b
    ldmfd   sp!, {r2, r3}               @ n, d
    mov     r0, r12
    mul     r1, r12, r3
    sub     r1, r2, r1                  @ remainder = n - q * d
    bx      lr

__aeabi_idiv:
__aeabi_idivmod:
    stmfd   sp!, {r4, lr}
    eor     r4, r0, r1              @ sign of quotient in bit 31
    and     r4, r4, #0x80000000
    orr     r4, r4, r0, lsr #31     @ sign of remainder (= sign of dividend) in bit 0
    cmp     r0, #0
    rsblt   r0, r0, #0
    cmp     r1, #0
    rsblt   r1, r1, #0
    bl      __aeabi_uidivmod
    tst     r4, #0x80000000
    rsbne   r0, r0, #0
    tst     r4, #1
    rsbne   r1, r1, #0
    ldmfd   sp!, {r4, lr}
    bx      lr

@ VBlank interrupt handler (installed at 0x03007FFC). BIOS enters in IRQ mode.
    .global irq_handler
    .align 2
irq_handler:
    mov     r0, #0x04000000
    ldr     r1, [r0, #0x200]        @ IE | IF<<16
    and     r1, r1, r1, lsr #16
    add     r2, r0, #0x200
    strh    r1, [r2, #2]            @ acknowledge IF
    ldr     r3, =0x03007FF8         @ BIOS IntrWait flags
    ldrh    r2, [r3]
    orr     r2, r2, r1
    strh    r2, [r3]
    tst     r1, #1
    bxeq    lr
    stmfd   sp!, {r0, lr}
    ldr     r3, =flip_req           @ page flip requested by the main loop?
    ldrb    r1, [r3]
    ldrh    r2, [r0]
    mov     r0, r2, lsr #4          @ headlight windows for the page shown from now (restarted every VBlank)
    and     r0, r0, #1
    eor     r0, r0, r1
    ldr     r3, =beam_vblank
    mov     lr, pc
    bx      r3
    ldmfd   sp!, {r0, lr}
    ldr     r3, =flip_req
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     1f
    mov     r2, #0
    strb    r2, [r3]
    ldrh    r2, [r0]                @ DISPCNT ^= frame select
    eor     r2, r2, #0x10
    strh    r2, [r0]
1:
    stmfd   sp!, {lr}
    ldr     r3, =sound_vblank
    mov     lr, pc
    bx      r3
    ldr     r3, =key_latch          @ remember every button seen this frame
    mov     lr, pc
    bx      r3
    ldmfd   sp!, {lr}
    bx      lr
    .pool
