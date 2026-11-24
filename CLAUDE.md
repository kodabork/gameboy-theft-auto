# GTA (Gameboy Theft Auto): New York

Original open-city game for the Game Boy Advance in the style of GTA: a pseudo-3D city, cars, peds, police, missions, cutscenes. The owner (GitHub: kodabork) is **not a coder**. Do all git, build and testing work for them, explain results in plain language, and hand them a ROM they can play.

## Hard rules
- **Never commit ROMs** (`*.gba`) or any commercial ROM. `.gitignore` already excludes `build/`, `*.gba`, `*.elf`, `*.o`.
- **Keep the repo private.** It uses GTA and Game Boy trademarks.
- **Cars are original look-alike archetypes** (Driv3r GBA style). Never replicate real licensed car designs.
- **Keep the title logo original.** It must not imitate the official GTA logo.
- **A crash alone must never kill the player.** Since v0.21 (the owner's request) staying inside a car until it explodes is fatal, so a burning player car always gets at least 4 s and can be bailed out of at any speed.
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
| `tools/gen_terrain.py` | The map: districts and block kinds (`city_kind`, `city_district`), coast ring, river mouths, radar map. Writes `build/terrain.bin`, `src/terrain.c/.h/.s`. Re-runs whenever it is newer than `build/terrain.bin`. |
| `src/host_main.c` | PC build for testing (see Testing). |

## Map (v0.30)
- 20 x 20 blocks of 64 m (`NB` in `world.h`, from `CITY_NB` in `terrain.h`); roads 0..NB at `ROAD_C(i)`. Coast ring around it, 1424 x 1424 terrain cells of 1 m.
- Districts: Docks/Old Town (blocks x<8, y<8, Act One), river (x=8), Harbor Park (y=8), the Heights (x<8, y>8), Downtown (x>8, y<10), Industrial (x>8, y>=10), Gull Island (x>=15, y>=15, channel at row/column 14).
- Block kinds: built, park, water. Water blocks are solid (terrain_m is WATER there; `water_push` pushes to the nearest quay).
- Everything per-frame is bucketed per block: `blk_start`/`tree_start` (world.c), lamps, props, roof props (game.c). Never loop over every building or prop.
- Parked cars: a pool of slots that follows the player (`park_spot`, `recycle_parked`, `PARKED_NEAR`).
- Host debug key `0x10000 + ((dir+1) << 16) + (bx << 8) + by`: drop the car on road `by` at cross street `bx`.

## Shops (v0.31)
- `setup_shops` in game.c: a table of (block, type); the door is on the block's east sidewalk (`x = bx*1024 + M(62.5)`, `y = by*1024 + M(30)`), skipped if the block is not built. Types: NY Guns, Corner Mart, Pawn & Loan, Motor Sales.
- Walking into the door beam (`FX_MARKER + 1`) switches to `GS_SHOP`; `shop_update` runs the buy menu and the hold-up. Robbery only when `mstate != 1`, armed, and the shop's cool-down is over.
- Art: `tools/make_interiors.py` ray-casts each room (boxes with simple textures), adds the clerk (a `Human` with a hands-up variant) and maps everything to the sprite palette into `src/interiors.c` (room, counter layer drawn over the clerk, clerk poses). `build.sh` re-runs it when the script or `src/assets.c` changes. The HUD draws shop text into the frame buffer (`text_f`), not as OBJ glyphs, because the menu needs more than 96 characters.
- Host debug key `0xF10 + i`: stand in front of shop i (also clears the wanted level and adds $5000).

## Memory budget
- **IWRAM:** the stack sits at 0x7E00. Keep `__bss_end` (`llvm-nm build/steelharbor.elf | grep __bss_end`) at or below about 0x7400 (v0.20: 0x6A5C, after `g_world` moved to EWRAM; v0.31: 0x6CDC, after the radar icons moved to ROM). render.o, collide.o and sound.o code lives in IWRAM; code that runs once or rarely goes in ROM with `__attribute__((section(".romcode")))`.
- Put big arrays in EWRAM with `EWRAM_BSS`.
- Thumb code has no fast divide. Avoid `/` in hot paths (reciprocal multiplies are used throughout) and avoid 64-bit division entirely.

## Testing
- **PC build:** run from `src/`:
  ```
  gcc -DHOST -O2 -w -o ../build/host host_main.c game.c world.c render.c collide.c tables.c assets.c hud.c terrain.c sound.c logo.c portraits.c
  ../build/host "frames:hexkeys,..." "dumpframes"
  ```
  Dumped frames go to `build/fNNNN.ppm`.
  - Keys: A=1, B=2, SELECT=4, START=8, RIGHT=0x10, LEFT=0x20, UP=0x40, DOWN=0x80, R=0x100, L=0x200.
  - Debug codes: 0xF00 set your own car on fire, 0xFD0 start play, 0xFE0+k teleport to job k's marker, 0xFFE give guns, 0xFC0+n damage the nearest car, 0xB00+n street, 0xC00+n coast.
- **Crash test:** build the PC build with `-fsanitize=address,undefined -fno-sanitize=shift-base` (fixed-point code shifts negative values on purpose) and fuzz with random key scripts.
- **Mission test:** `tools/mission_test.c` plays every job's win condition in order. Build it from `src/` with `gcc -DHOST -O1 -w -I. -o mt ../tools/mission_test.c world.c render.c collide.c tables.c assets.c hud.c terrain.c sound.c logo.c portraits.c`. `tools/crash_test.c` is the high-speed car collision test.
- **ROM profiler and emulator:** `tools/gbaprof.c`. Build it with `gcc -O2 -o tools/gbaprof tools/gbaprof.c`.
  - Usage: `gbaprof rom seconds "frames:keys,..." [dump_flips] [prefix]`. It prints fps.
  - `PROF=file PROF_FROM=sec` writes per-PC cycles; aggregate them with `tools/profsym.py` and `tools/byline.py`.
  - `SRAM_FILE=path` keeps save memory between runs (used for the save, reboot, CONTINUE test).
  - `FIXDT=819` fixes the game clock and indexes keys by displayed frame, so different builds play the identical game; `HIST=a-b` prints VBlanks per frame and fps over displayed frames a..b.
  - **Benchmark:** `tools/bench.sh [rom]` (needs `build/bench.txt`, the standard drive script in displayed frames). v0.19: 14.4 fps, v0.20: 18.2 fps, v0.24: 19.0 fps, v0.30 (big map): 19.2 fps.

## Sound
- `src/sound.c`: DirectSound mixer (engine, horn, skid, guns, siren, effect synth) and PSG radio. Five stations (`stations[]`): 64-step patterns for lead, chord stabs, bass and drums.
- Effects: `g_snd.sfx |= SFX_*` from game code. Each effect is a short note list (`SfxNote`) played on 4 synth voices. `tools/sfx_wav.c` renders every effect, and every gun with its follow-up (`SFX_SHELL`, `SFX_PUMP`, `SFX_BOLT`, `SFX_LAUNCH`), to WAV with the real mixer. Gun recipes are in `recipes[]`: 1 pistol, 2 SMG, 3 shotgun, 4 rifle, 5 explosion, 6 rocket launch.

## People
- `tools/sprites_people.py` (skeleton + gait curves). Per set: 9 stored view directions of 16 (the rest mirrored), 26 frames each (idle, walk 12, run 12, aim), 8 down frames, 24 ragdoll tumble frames (3 views x 8 pitch angles). Offsets `PED_DIR_FRAMES`, `PED_DOWN`, `PED_TUMBLE`, `PED_BLOCK` (far set) come from `assets.h`.
- v0.25 body (owner said the old one was stiff and lanky, with odd bits on the arms): no shoulder balls (deltoid ellipsoids plus a trapezius cone from the neck), fists instead of flat hand ovals, fuller limbs. Arms swing *opposite* the same-side leg (`sh` in `GAIT` is used directly), hips and shoulders twist (`tw`), and running elbows stay bent at about 1.15-1.35 rad.
- Weapon stances: `Human(mode='aim', weapon=...)` with 'pistol' (the normal aim frame) or 'smg', 'shotgun', 'rifle', 'rocket' (hands placed on the weapon by 2-bone IK, `GUNS`, `_gun_sdf`). These are player-only frames stored after the player block: `PED_PLAYER + PED_STANCE + dir * PED_NSTANCE + (weapon - 2)`.
- Ragdolls: `update_ragdoll` in `game.c` (velocity, gravity, bounces, wall rebounds, slide, settle); `kill_ped` launches them.

## Car models
- Headlights: one design per vehicle class (`lk` map in `Car3.shade`). The owner is very sensitive to "droopy" lamps: keep top and bottom edges sweeping UP toward the outside (currently 7 cm over the lamp width) and stay on the flat part of the nose, because the game's raised 3/4 camera makes level lamps look like they sag. Check with a straight-on and a 3/4 render before showing anything.
- `tools/cars3.py` builds each car from side profiles (height fields). Profile distances are scaled by `1/sqrt(1+slope^2)` (`kfac`) so steep faces (tailgates, upright rear glass) render cleanly. The box truck's cargo box is a separate rounded box (`box_x`, `box_z`); the pickup cab's back wall is a plane.
- Preview a model quickly with `tools/frame_render.render_frame` before running the full sprite render.

## Story and missions
See `STORY.md`. There are nine jobs; the races were removed in v0.19. Cutscenes run in-engine: the giver actor stands in ped slot `PED_ACTOR`, the camera logic is in `game_build_scene`. The dialogue box is in `hud.c`. **Portraits are switched off (`SHOW_PORTRAITS 0`) at the owner's request:** five code-drawn styles (flat pixel, 3D-rendered, painted, anime, SNES) were all rejected. Don't try to generate faces in code again; if the owner supplies art, convert it. The plumbing still works: an 8bpp 64x64 OBJ at tiles 896.., colours in OBJ palette entries 96..255, OAM 127, uploaded when the speaker changes (`src/portraits.c`).

## Ideas not done yet
- More performance work: walls (`band`/`vspan`) and sprite blits are now the largest costs.
- A finer ground texture (0.5 m texels look stair-stepped close up).
- Female ped models for Marta and Jinx. They currently reuse outfit colours (their portraits are female).
