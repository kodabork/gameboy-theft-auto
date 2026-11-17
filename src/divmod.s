@ Integer division helpers + VBlank interrupt handler. ARM code in IWRAM.
    .section .iwram, "ax"
    .arm
    .align 2
    .global __aeabi_idiv, __aeabi_idivmod, __aeabi_uidiv, __aeabi_uidivmod

@ Fast shift-subtract division: only loops over the quotient's bits.
@ r0 = numerator, r1 = denominator  ->  r0 = quotient, r1 = remainder
__aeabi_uidiv:
__aeabi_uidivmod:
    cmp     r1, #0
    moveq   r0, #0
    bxeq    lr
    mov     r2, r1                  @ shifted divisor
    mov     r3, #1                  @ current quotient bit
1:  cmp     r2, r0, lsr #1
    movls   r2, r2, lsl #1
    movls   r3, r3, lsl #1
    bls     1b
    mov     r12, #0
2:  cmp     r0, r2
    subcs   r0, r0, r2
    orrcs   r12, r12, r3
    movs    r3, r3, lsr #1
    movne   r2, r2, lsr #1
    bne     2b
    mov     r1, r0                  @ remainder
    mov     r0, r12                 @ quotient
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
    ldr     r3, =flip_req           @ page flip requested by the main loop?
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
