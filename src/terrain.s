    .section .rodata.terrain, "a"
    .global terrain_m
    .align 2
terrain_m:
    .incbin "build/terrain.bin", 0, 2027776
    .global terrain_r
terrain_r:
    .incbin "build/terrain.bin", 2027776, 2027776
    .global radar_map
radar_map:
    .incbin "build/terrain.bin", 4055552, 506944
