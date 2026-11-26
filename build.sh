#!/bin/sh
# Builds steelharbor.gba with clang + lld (no devkitPro needed).
#   LOGO_ROM=/path/to/a_gba_rom_you_own.gba ./build.sh        (logo needed for real hardware)
#   REGEN_ASSETS=1 ./build.sh                                  (re-render all sprites, ~40 min)
set -e
cd "$(dirname "$0")"
mkdir -p build
python3 tools/gen_tables.py src/tables.c
[ -f build/terrain.bin ] && [ build/terrain.bin -nt tools/gen_terrain.py ] || python3 tools/gen_terrain.py build/terrain.bin src/terrain.c src/terrain.h
[ -f build/lanez.bin ] && [ build/lanez.bin -nt build/terrain.bin ] && [ build/lanez.bin -nt tools/gen_lanez.py ] || python3 tools/gen_lanez.py build/terrain.bin build/lanez.bin
if [ -n "$REGEN_ASSETS" ] || [ ! -f build/assets.bin ]; then
    python3 tools/make_assets.py src/assets.c build/assets.bin
    python3 tools/append_props.py src/assets.c src/assets.h build/assets.bin
    python3 tools/append_fx.py src/assets.c src/assets.h build/assets.bin
fi
CF="$EXTRA_CF -g --target=arm-none-eabi -mcpu=arm7tdmi -mfloat-abi=soft -O2 -ffreestanding -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -Wall -Wno-unused-function"
python3 tools/make_logo.py src/assets.c src/logo.c build/logo_preview.png > /dev/null
python3 tools/make_portraits.py src/assets.c src/portraits.c > /dev/null
[ src/interiors.c -nt tools/make_interiors.py ] && [ src/interiors.c -nt src/assets.c ] || python3 tools/make_interiors.py src/assets.c src/interiors.c build/interiors_preview.png > /dev/null
for f in game world tables gba_main libc assets hud terrain logo portraits interiors minigames minigame_art; do clang $CF $ROMOPT -mthumb -c src/$f.c -o build/$f.o; done
for f in render collide sound; do clang $CF -marm -c src/$f.c -o build/$f.o; done
for f in crt0 divmod assets_bin; do :; done
clang --target=arm-none-eabi -mcpu=arm7tdmi -c src/crt0.s -o build/crt0.o
clang --target=arm-none-eabi -mcpu=arm7tdmi -c src/divmod.s -o build/divmod.o
clang --target=arm-none-eabi -mcpu=arm7tdmi -c src/vfill.s -o build/vfill.o
clang --target=arm-none-eabi -mcpu=arm7tdmi -c src/assets.s -o build/assets_bin.o
clang --target=arm-none-eabi -mcpu=arm7tdmi -c src/terrain.s -o build/terrain_bin.o
clang --target=arm-none-eabi -mcpu=arm7tdmi -c src/lanez.s -o build/lanez_bin.o
ld.lld -T src/gba.ld -o build/steelharbor.elf build/crt0.o build/vfill.o build/gba_main.o build/game.o build/world.o \
    build/render.o build/tables.o build/divmod.o build/libc.o build/assets.o build/assets_bin.o build/sound.o build/hud.o build/collide.o build/terrain.o build/terrain_bin.o build/lanez_bin.o build/logo.o build/portraits.o build/interiors.o build/minigames.o build/minigame_art.o
llvm-objcopy -O binary build/steelharbor.elf build/steelharbor.gba
python3 tools/fixrom.py build/steelharbor.gba "$LOGO_ROM"
echo "built build/steelharbor.gba"
