# Roadmap

This is the plan the owner asked to queue up (written after v0.25). Work starts on the **Next up** list.

## Where we stand (v0.25)
| Budget | Used | Limit | Notes |
|---|---|---|---|
| ROM | about 16.5 MB | 32 MB (GBA maximum) | Sprites are about 15 MB of it. |
| Terrain map | 1.4 MB | | 656 x 656 cells at 0.5 m, so the playable map is about 330 m across. |
| IWRAM | `__bss_end` about 0x72B0 | about 0x7400 | Almost full. New per-frame state goes in EWRAM. |
| EWRAM | | 256 KB | Holds `g_world` and the big arrays. |
| Frame rate | 19.0 fps on `tools/bench.sh` | | Walls, ground and sprite blits are about half the frame. |

## Next up (owner's requests)
1. **Make the map 4-7 times bigger** (to roughly 650-870 m across).
   - The terrain is stored raw (height, material and road bytes per 0.5 m cell). Seven times the area stored the same way is about 10 MB. That would fit, but it leaves little room for anything else.
   - Plan: store the map as compressed 16 x 16 m chunks. City blocks repeat a lot (roads, pavements, flat lots), so a chunk dictionary plus a chunk index should cost about 1-3 MB.
   - The renderer and collision already read cells through `terrain_*` lookups. Keep a small decompressed cache of chunks in EWRAM around the camera, so per-pixel cost stays the same.
   - Grow `g_world` (buildings, lamps, trees, props) to match. Lamps are already bucketed by block, and every per-frame list should be bucketed the same way, so cost depends on what is near the camera, not on map size.
   - The radar minimap scrolls over the bigger map. It already samples a rotated window, so this needs a bigger `radar_map` (or a 2x-coarser one).
   - Split the city into districts (docks, downtown, hills, industrial, suburbs, an island) that unlock as the story goes, the way classic open-world games do with bridges.
2. **A real story of about 6 hours.**
   - Text and mission scripts cost only kilobytes, so story length is not limited by ROM.
   - About 6 hours means roughly 35-45 story missions in 3-4 acts, plus side content. Today there are 9 jobs.
   - Write `STORY.md` first: characters, acts, district unlock order. Then build missions on the existing objective types (drive to, chase, destroy, collect, escort, survive the wanted level), plus a few new ones: tail a car without being seen, timed delivery, defend a spot, photograph/spot.
   - Cutscenes keep the in-engine actor and letterbox system. Portraits stay off unless the owner supplies art.
3. **Stores and sellers.**
   - Gun shop (ammo, armour), food stand (health), car respray (loses the wanted level, changes colour from the existing paint sets), and a clothes shop.
   - Shops are walk-in markers on the map with a menu. That reuses the pause-menu UI and costs no sprites.
   - Clothes: each new full player outfit is a new player sprite set (about 300 frames, several hundred KB). Recolours could instead reuse the same frames with a palette swap, which is far cheaper. Decide when we get there.
4. **People and police react to each other.**
   - Peds can commit crimes (mugging, carjacking, fights, running red lights). Nearby cops chase and arrest or shoot them, and peds flee, fight back or watch.
   - Performance: evaluate a few peds per frame in round-robin rather than every ped every frame, so the CPU cost stays flat. Same number of peds and cars on screen as today.

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
- Building interiors as real 3D spaces. Shops should be menus at the door.
- Taller or much denser buildings everywhere. Wall drawing is already the biggest per-frame cost.
- Female ped models are fine for ROM (a new ped set is about 1 MB with near and far frames), but keep the total number of sets in check.
