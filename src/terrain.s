    .section .rodata.terrain, "a"
    .global radar_map
    .align 2
radar_map:
    .incbin "build/terrain.bin", 4055552, 506944
