# Roadmap

This is the plan the owner asked to queue up. Work goes down the **Next up** list in order.

## Where we stand (v0.30)
| Budget | Used | Limit | Notes |
|---|---|---|---|
| ROM | about 20.3 MB | 32 MB (GBA maximum) | Sprites are about 15 MB of it. |
| Terrain map | 4.6 MB | | 1424 x 1424 cells at 1 m: material and road bytes, plus the radar map. |
| IWRAM | `__bss_end` 0x73AC | about 0x7400 | Almost full. New per-frame state goes in EWRAM. |
| EWRAM | about 150 KB | 256 KB | `g_world` (up to 1600 buildings), props, lamps, roof props. |
| Frame rate | 19.2 fps on `tools/bench.sh` | | Walls, ground and sprite blits are about half the frame. |

## Done
- **Bigger map (v0.30):** 20 x 20 blocks of 64 m (1.3 km), six districts, river and island channel.
  Everything per-frame is bucketed by block (buildings, props, lamps, park trees, roof props), so
  the cost depends only on what is near the camera.

- **Shops and robberies (v0.31):** 23 walk-in shops (gun shop, corner store, pawn shop, car showroom) with
  pre-rendered interiors and clerks (`tools/make_interiors.py`), buy menus, and free-roam hold-ups.
  Later: clothes shop, respray, food stands, and shop-based story missions.

- **Bowling and pool (v0.32):** `src/minigames.c` (bowling with pin physics and a pin cam, 8-ball with a
  CPU hustler), art from `tools/make_interiors.py` into `src/minigame_art.c`. Venues use the shop doors.

- **The full story (v0.33):** 40 missions in four acts on a scripted mission engine (`src/story_acts.h`).
  District locks were not added: every district is open from the start.

- **Street crime (v0.34):** muggings, fights and carjackings near the player; cops chase, arrest or shoot,
  patrol cars run down carjackers; killing a criminal pays a vigilante reward.

## Next up (owner's requests)
Nothing queued. Pick from the extras below, or ask for something new.

## Recommended extras (small ROM cost, little or no frame-rate cost)
- **Side jobs with vehicles we already have:** taxi fares, ambulance runs, police vigilante, and car-thief export lists. No new art; mostly mission logic.
- **Safehouses and garages:** save points across the bigger map, and keep a stolen car in a garage. Logic only.
- **Day/night cycle:** done with palette fades (the whole screen is one 256-colour palette), so it costs almost nothing per frame. Street lamps already exist. Light-up windows at night can come from the same palette trick.
- **Wanted levels 1-5:** roadblocks (parked police cars), and more aggressive police at high levels. Uses the existing police car, van and box truck art.
- **Hidden packages / collectibles** (100 across the city, rewards at milestones), **unique stunt jumps**, a **stats screen** and **in-game achievements**. All tiny.
- **More radio:** more stations and songs (each station is a few KB of patterns), plus scrolling DJ/advert text when you tune in.
- **Phone calls / pager messages** that start missions and keep the story going between cutscenes. Text only.
- **Money sinks and progress:** buy properties that pay a little income and unlock safehouses.
- **More car variety by recolouring:** a new paint scheme for an existing model is cheap if done as palette variants. A brand-new car model costs ROM for every angle, so only add a few of those.
- **Boats on the coast:** the yacht model already exists. Needs water driving physics and docks.
- **Armour pickups and a melee punch for fists:** fists can't hit anything today. That needs a punch animation, about 9 directions x 3 frames for the player only.

## Things to avoid (they would hurt the frame rate)
- More people or cars on screen at once than today. Each extra sprite costs blit time.
- Transparency or blending effects over the 3D view (Mode 4 has no cheap per-pixel blending).
- Full-screen weather that draws over every pixel. Light rain as a handful of HUD sprites (OBJ layer) would be fine.
- Building interiors as real 3D spaces. Shops use a separate 2D interior screen instead.
- Taller or much denser buildings everywhere. Wall drawing is already the biggest per-frame cost.
- Female ped models are fine for ROM (a new ped set is about 1 MB with near and far frames), but keep the total number of sets in check.
