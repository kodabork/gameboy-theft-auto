@ Lane-line zone per block of road samples (tools/gen_lanez.py), read by vfill.s gmiss.
    .section .rodata.lanez, "a"
    .global lane_zones
lane_zones:
    .incbin "build/lanez.bin"
