    .section .rodata.terrain, "a"
    .global terrain_m
    .align 2
terrain_m:
    .incbin "build/terrain.bin", 430336, 430336
    .global terrain_r
terrain_r:
    .incbin "build/terrain.bin", 860672, 430336
    .global radar_map
radar_map:
    .incbin "build/terrain.bin", 1291008, 107584
