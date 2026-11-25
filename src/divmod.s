@ Integer division helpers + VBlank interrupt handler. ARM code in IWRAM.
    .section .iwram, "ax"
    .arm
    .align 2
    .global __aeabi_idiv, __aeabi_idivmod, __aeabi_uidiv, __aeabi_uidivmod

@ Unsigned division: find how many quotient bits there are (binary search on n >> k >= d), then jump
@ into a fully unrolled shift-subtract table, 3 instructions per quotient bit.
@ r0 = numerator, r1 = denominator  ->  r0 = quotient, r1 = remainder   (d = 0 gives q = 0)
__aeabi_uidiv:
__aeabi_uidivmod:
    cmp     r1, #0
    moveq   r0, #0
    bxeq    lr
    cmp     r0, r1
    movlo   r1, r0                  @ n < d: quotient 0, remainder n
    movlo   r0, #0
    bxlo    lr
    mov     r2, #0                  @ s = largest k with (d << k) <= n
    mov     r3, r0
    cmp     r1, r3, lsr #16
    movls   r3, r3, lsr #16
    addls   r2, r2, #16
    cmp     r1, r3, lsr #8
    movls   r3, r3, lsr #8
    addls   r2, r2, #8
    cmp     r1, r3, lsr #4
    movls   r3, r3, lsr #4
    addls   r2, r2, #4
    cmp     r1, r3, lsr #2
    movls   r3, r3, lsr #2
    addls   r2, r2, #2
    cmp     r1, r3, lsr #1
    addls   r2, r2, #1
    mov     r3, #0                  @ quotient
    rsb     r2, r2, #31
    add     r2, r2, r2, lsl #1      @ (31 - s) * 3 instructions
    add     pc, pc, r2, lsl #2
    nop
    cmp     r0, r1, lsl #31
    subcs   r0, r0, r1, lsl #31
    adc     r3, r3, r3
    cmp     r0, r1, lsl #30
    subcs   r0, r0, r1, lsl #30
    adc     r3, r3, r3
    cmp     r0, r1, lsl #29
    subcs   r0, r0, r1, lsl #29
    adc     r3, r3, r3
    cmp     r0, r1, lsl #28
    subcs   r0, r0, r1, lsl #28
    adc     r3, r3, r3
    cmp     r0, r1, lsl #27
    subcs   r0, r0, r1, lsl #27
    adc     r3, r3, r3
    cmp     r0, r1, lsl #26
    subcs   r0, r0, r1, lsl #26
    adc     r3, r3, r3
    cmp     r0, r1, lsl #25
    subcs   r0, r0, r1, lsl #25
    adc     r3, r3, r3
    cmp     r0, r1, lsl #24
    subcs   r0, r0, r1, lsl #24
    adc     r3, r3, r3
    cmp     r0, r1, lsl #23
    subcs   r0, r0, r1, lsl #23
    adc     r3, r3, r3
    cmp     r0, r1, lsl #22
    subcs   r0, r0, r1, lsl #22
    adc     r3, r3, r3
    cmp     r0, r1, lsl #21
    subcs   r0, r0, r1, lsl #21
    adc     r3, r3, r3
    cmp     r0, r1, lsl #20
    subcs   r0, r0, r1, lsl #20
    adc     r3, r3, r3
    cmp     r0, r1, lsl #19
    subcs   r0, r0, r1, lsl #19
    adc     r3, r3, r3
    cmp     r0, r1, lsl #18
    subcs   r0, r0, r1, lsl #18
    adc     r3, r3, r3
    cmp     r0, r1, lsl #17
    subcs   r0, r0, r1, lsl #17
    adc     r3, r3, r3
    cmp     r0, r1, lsl #16
    subcs   r0, r0, r1, lsl #16
    adc     r3, r3, r3
    cmp     r0, r1, lsl #15
    subcs   r0, r0, r1, lsl #15
    adc     r3, r3, r3
    cmp     r0, r1, lsl #14
    subcs   r0, r0, r1, lsl #14
    adc     r3, r3, r3
    cmp     r0, r1, lsl #13
    subcs   r0, r0, r1, lsl #13
    adc     r3, r3, r3
    cmp     r0, r1, lsl #12
    subcs   r0, r0, r1, lsl #12
    adc     r3, r3, r3
    cmp     r0, r1, lsl #11
    subcs   r0, r0, r1, lsl #11
    adc     r3, r3, r3
    cmp     r0, r1, lsl #10
    subcs   r0, r0, r1, lsl #10
    adc     r3, r3, r3
    cmp     r0, r1, lsl #9
    subcs   r0, r0, r1, lsl #9
    adc     r3, r3, r3
    cmp     r0, r1, lsl #8
    subcs   r0, r0, r1, lsl #8
    adc     r3, r3, r3
    cmp     r0, r1, lsl #7
    subcs   r0, r0, r1, lsl #7
    adc     r3, r3, r3
    cmp     r0, r1, lsl #6
    subcs   r0, r0, r1, lsl #6
    adc     r3, r3, r3
    cmp     r0, r1, lsl #5
    subcs   r0, r0, r1, lsl #5
    adc     r3, r3, r3
    cmp     r0, r1, lsl #4
    subcs   r0, r0, r1, lsl #4
    adc     r3, r3, r3
    cmp     r0, r1, lsl #3
    subcs   r0, r0, r1, lsl #3
    adc     r3, r3, r3
    cmp     r0, r1, lsl #2
    subcs   r0, r0, r1, lsl #2
    adc     r3, r3, r3
    cmp     r0, r1, lsl #1
    subcs   r0, r0, r1, lsl #1
    adc     r3, r3, r3
    cmp     r0, r1, lsl #0
    subcs   r0, r0, r1, lsl #0
    adc     r3, r3, r3
    mov     r1, r0                  @ remainder
    mov     r0, r3                  @ quotient
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
