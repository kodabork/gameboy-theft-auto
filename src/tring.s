@ Packed terrain maps (tools/pack_terrain.py): coast ring of terrain_m and terrain_r, interior water bits,
@ and the ring row tables vfill.s uses.
    .section .rodata.tring, "a"
    .global tring_m, tring_r, tring_water, ring_lo, ring_hi
    .align 2
tring_m:
    .incbin "build/tring.bin", 0, 353340
tring_r:
    .incbin "build/tring.bin", 353340, 353340
tring_water:
    .incbin "build/tring.bin", 706680, 209628
    .align 2
ring_lo:
    .incbin "build/tring.bin", 916308, 5696
ring_hi:
    .incbin "build/tring.bin", 922004, 5696
