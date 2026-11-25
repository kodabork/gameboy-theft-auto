    .section .rodata.assets, "a"
    .global spr_pixels
    .align 2
spr_pixels:
    .incbin "build/sprpack.bin"
