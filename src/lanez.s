@ Lane-line lookup tables for the coast ring (tools/gen_lanez.py), read by vfill.s gmiss.
    .section .rodata.lanez, "a"
    .global lane_ring, lane_side
    .align 2
lane_ring:
    .incbin "build/lanering.bin"
    .align 2
lane_side:
    .incbin "build/laneside.bin"
