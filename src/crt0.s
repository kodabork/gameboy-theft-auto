@ Steel Harbor GBA startup
    .section .crt0, "ax"
    .arm
    .global _start
_start:
    b       start2
    .space  0xBC                @ cartridge header, filled in by tools/fixrom.py
start2:
    mov     r0, #0x12           @ IRQ mode stack
    msr     cpsr_c, r0
    ldr     sp, =0x03007FA0
    mov     r0, #0x1F           @ System mode stack
    msr     cpsr_c, r0
    ldr     sp, =0x03007E00
    ldr     r0, =__iwram_lma    @ copy IWRAM code+data from ROM
    ldr     r1, =__iwram_start
    ldr     r2, =__iwram_end
1:  cmp     r1, r2
    ldrlt   r3, [r0], #4
    strlt   r3, [r1], #4
    blt     1b
    mov     r3, #0
    ldr     r1, =__bss_start    @ clear IWRAM bss
    ldr     r2, =__bss_end
2:  cmp     r1, r2
    strlt   r3, [r1], #4
    blt     2b
    ldr     r1, =__ewbss_start  @ clear EWRAM bss
    ldr     r2, =__ewbss_end
3:  cmp     r1, r2
    strlt   r3, [r1], #4
    blt     3b
    ldr     r0, =main
    mov     lr, pc
    bx      r0
    b       .
    .pool
