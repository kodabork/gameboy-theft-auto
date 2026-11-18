# GTA (Gameboy Theft Auto): Steel Harbor

Original open-city game for the Game Boy Advance in the style of GTA: a pseudo-3D city, cars, peds, police, missions, cutscenes. The owner (GitHub: kodabork) is **not a coder**. Do all git, build and testing work for them, explain results in plain language, and hand them a ROM they can play.

## Hard rules
- **Never commit ROMs** (`*.gba`) or any commercial ROM. `.gitignore` already excludes `build/`, `*.gba`, `*.elf`, `*.o`.
- **Keep the repo private.** It uses GTA and Game Boy trademarks.
- **Cars are original look-alike archetypes** (Driv3r GBA style). Never replicate real licensed car designs.
- **Keep the title logo original.** It must not imitate the official GTA logo.
- **Crashes must never kill the player.**
- Commit messages end with a `Co-Authored-By` trailer for the model in use.
- Bump the version and add a "What's new" section to `README.md` with each release.

## Build
Toolchain: clang (arm7tdmi target), ld.lld, llvm-objcopy, python3 with numpy and Pillow. No devkitPro.

```
LOGO_ROM=/path/to/a/gba/rom/you/own.gba ./build.sh     # writes build/steelharbor.gba
```

- **Boot logo:** `LOGO_ROM` supplies the Nintendo boot logo bytes. The owner used their own Grand Theft Auto Advance ROM. Emulators boot without it; real hardware needs it. Ask the owner for the file if real-hardware testing matters.
- **Fresh clone:** `build/` is empty, so the first build generates `build/terrain.bin` (quick) and renders every sprite into `build/assets.bin`. The sprite render runs on every CPU core (about 7 minutes on 4 cores) and caches each frame in `build/cache/`, so re-running after a model change only re-renders what changed. Run it in the background:
  ```
  nohup sh -c 'python3 tools/make_assets.py src/assets.c build/assets.bin && \
    python3 tools/append_props.py src/assets.c src/assets.h build/assets.bin && \
    python3 tools/append_fx.py src/assets.c src/assets.h build/assets.bin && echo DONE' \
    > build/regen.log 2>&1 &
  ```
  `src/assets.c` and `src/assets.h` are committed, but they must match `build/assets.bin`. Always run all three steps together.
- **Header:** title `GBTHEFTAUTO`, code `SHRB`, written by `tools/fixrom.py`. The save type tag `SRAM_V113` lives in `gba_main.c`.
- **Delivering:** copy `build/steelharbor.gba` to the owner as `gameboy-theft-auto.gba`. Their Mac folder is `/Users/parkermoore/GBA GTA`.

## Layout
| File | What it holds |
|---|---|
| `src/game.c` | All game logic: player, cars, traffic, police, peds, weapons, damage and explosions, missions and story, cutscenes, save, menus. Thumb code in ROM. |
| `src/render.c` | Mode 4 renderer (120x160 logical): floor-cast ground, per-column walls, sky, sprite blit with mirror flip (bit 15 of the frame number). ARM code in IWRAM. |
| `src/collide.c`, `src/sound.c` | ARM code in IWRAM. `sound.c` is the mixer. |
| `src/hud.c` | HUD: OBJ 8x8 text glyphs, framebuffer rects, radar, title, pause menu, dialogue/letterbox. |
| `src/gba_main.c` | Main loop. The page flip happens in the VBlank IRQ (`flip_req`, `divmod.s`). Buttons are latched every VBlank (`key_latch`). |
| `tools/cars3.py` | Car models, rendered by `tools/sprites_sdf.py`. Only headings 0..n/2 are stored; the rest are drawn mirrored. |
| `tools/make_assets.py`, `append_props.py`, `append_fx.py` | The sprite pipeline. |
| `tools/sheet.py` | Car contact sheet. |
| `src/host_main.c` | PC build for testing (see Testing). |

## Memory budget
- **IWRAM:** the stack sits at 0x7E00. Keep `__bss_end` (`llvm-nm build/steelharbor.elf | grep __bss_end`) at or below about 0x7400 (v0.20: 0x6A5C, after `g_world` moved to EWRAM). render.o, collide.o and sound.o code lives in IWRAM; code that runs once or rarely goes in ROM with `__attribute__((section(".romcode")))`.
- Put big arrays in EWRAM with `EWRAM_BSS`.
- Thumb code has no fast divide. Avoid `/` in hot paths (reciprocal multiplies are used throughout) and avoid 64-bit division entirely.

## Testing
- **PC build:** run from `src/`:
  ```
  gcc -DHOST -O2 -w -o ../build/host host_main.c game.c world.c render.c collide.c tables.c assets.c hud.c terrain.c sound.c logo.c
  ../build/host "frames:hexkeys,..." "dumpframes"
  ```
  Dumped frames go to `build/fNNNN.ppm`.
  - Keys: A=1, B=2, SELECT=4, START=8, RIGHT=0x10, LEFT=0x20, UP=0x40, DOWN=0x80, R=0x100, L=0x200.
  - Debug codes: 0xFD0 start play, 0xFE0+k teleport to job k's marker, 0xFFE give guns, 0xFC0+n damage the nearest car, 0xB00+n street, 0xC00+n coast.
- **Crash test:** build the PC build with `-fsanitize=address,undefined` and fuzz with random key scripts.
- **Mission test:** `tools/mission_test.c` plays every job's win condition in order. Build it from `src/` with `gcc -DHOST -O1 -w -I. -o mt ../tools/mission_test.c world.c render.c collide.c tables.c assets.c hud.c terrain.c sound.c logo.c`. `tools/crash_test.c` is the high-speed car collision test.
- **ROM profiler and emulator:** `tools/gbaprof.c`. Build it with `gcc -O2 -o tools/gbaprof tools/gbaprof.c`.
  - Usage: `gbaprof rom seconds "frames:keys,..." [dump_flips] [prefix]`. It prints fps.
  - `PROF=file PROF_FROM=sec` writes per-PC cycles; aggregate them with `tools/profsym.py` and `tools/byline.py`.
  - `SRAM_FILE=path` keeps save memory between runs (used for the save, reboot, CONTINUE test).
  - `FIXDT=819` fixes the game clock and indexes keys by displayed frame, so different builds play the identical game; `HIST=a-b` prints VBlanks per frame and fps over displayed frames a..b.
  - **Benchmark:** `tools/bench.sh [rom]` (needs `build/bench.txt`, the standard drive script in displayed frames). v0.19: 14.4 fps, v0.20: 18.2 fps.

## Sound
- `src/sound.c`: DirectSound mixer (engine, horn, skid, guns, siren, effect synth) and PSG radio. Five stations (`stations[]`): 64-step patterns for lead, chord stabs, bass and drums.
- Effects: `g_snd.sfx |= SFX_*` from game code. Each effect is a short note list (`SfxNote`) played on 4 synth voices. `tools/sfx_wav.c` renders every effect to WAV with the real mixer.

## Car models
- `tools/cars3.py` builds each car from side profiles (height fields). Profile distances are scaled by `1/sqrt(1+slope^2)` (`kfac`) so steep faces (tailgates, upright rear glass) render cleanly. The box truck's cargo box is a separate rounded box (`box_x`, `box_z`); the pickup cab's back wall is a plane.
- Preview a model quickly with `tools/frame_render.render_frame` before running the full sprite render.

## Story and missions
See `STORY.md`. There are nine jobs; the races were removed in v0.19. Cutscenes run in-engine: the giver actor stands in ped slot `PED_ACTOR`, the camera logic is in `game_build_scene`, and the letterbox is in `hud.c`.

## Ideas not done yet
- More performance work: walls (`band`/`vspan`) and sprite blits are now the largest costs.
- A finer ground texture (0.5 m texels look stair-stepped close up).
- Female ped models for Marta and Jinx. They currently reuse outfit colours.
