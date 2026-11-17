OUTPUT_ARCH(arm)
ENTRY(_start)
MEMORY {
    ROM   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 0x7A00
    EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
}
SECTIONS {
    .text : {
        KEEP(*(.crt0))
        *(EXCLUDE_FILE(*render.o *divmod.o *collide.o *sound.o) .text .text.*)
        *(EXCLUDE_FILE(*render.o *collide.o) .rodata .rodata.*)
        . = ALIGN(4);
    } > ROM
    .iwram : {
        __iwram_start = .;
        *render.o(.text .text.* .rodata .rodata.*)
        *collide.o(.text .text.* .rodata .rodata.*)
        *sound.o(.text .text.*)
        *(.iwram .iwram.*)
        *(.data .data.*)
        . = ALIGN(4);
        __iwram_end = .;
    } > IWRAM AT > ROM
    __iwram_lma = LOADADDR(.iwram);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > IWRAM
    .ewram (NOLOAD) : {
        __ewbss_start = .;
        *(.ewram .ewram.*)
        . = ALIGN(4);
        __ewbss_end = .;
    } > EWRAM
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
