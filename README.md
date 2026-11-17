# GTA (Gameboy Theft Auto): Steel Harbor

An original open-city game for Game Boy Advance, with a third-person chase camera,
written from scratch in C. It's inspired by the GTA IV look but has no Rockstar code or assets.

## Controls
| | In a car | On foot |
|---|---|---|
| A | Gas | Fire (auto-aims at the nearest person in front) |
| B | Brake / reverse; B + steer at speed = handbrake drift | Run (hold with Up) |
| L | Nitro | Switch weapon |
| Left / Right | Steer | Turn |
| Up / Down | – | Walk forward / back |
| R | Get out (when slow) | Get in the nearest car (any car) |
| SELECT | Horn | |
| START | Pause menu (resume, save, radio, quit) | |
| SELECT + START | Toggle FPS counter | |

## What's new in v0.19
- **New story and missions.** The races are gone; all nine jobs are now crime jobs, tied together by
  one story (see the table below)
- **Cutscenes.** Every job starts with an in-engine cutscene at its marker: letterbox bars, the
  mission giver standing in the street facing Eddie, an establishing shot, then over-the-shoulder
  cuts to whoever is talking. The game opens and ends the same way
- **Objective text moved** up into the sky, off the road, and the checkpoint counter is gone
- Button taps are latched every screen refresh, so a quick press in a cutscene or menu is never lost

| # | Job | Giver | What you do | Pay |
|---|-----|-------|-------------|-----|
| 1 | Hot Wheels | Marta | Steal a yellow supercar and bring it to her garage | $500 |
| 2 | Candy Run | Jinx | Collect a package at the docks; the cops see the handoff; get it to the club in 100 s | $800 |
| 3 | The Bookkeeper | Moran | Kill the bookkeeper in the suit and get past his three bodyguards | $1,500 |
| 4 | Wrong Corner | Moran | Burn the Eastside Kings' three cars on Moran's corner while they shoot back | $1,800 |
| 5 | Dirty Blue | Shaw | Kill two honest cops at the old mill, then lose the 3-star heat | $2,200 |
| 6 | Special Delivery | Moran | Drive a van of product to the east coast road in 90 s | $2,500 |
| 7 | The Ledger | Jinx | Moran's men come for Jinx: kill all three, lose the cops, get to the safehouse | $3,000 |
| 8 | Cash Run | Marta | Ride Shaw's SUV until he bails, then finish him (or just wreck the SUV) | $3,500 |
| 9 | Last Call | Rudy | Take Moran and his guards down at the old pier, with cops on the way | $10,000 + ending |

## v0.18
- **Car crashes at high speed:** car movement is now split into small sub-steps whenever anything
  nearby is moving fast, with collisions checked after each one, so a fast car can no longer skip
  through another car between frames (tested: 0 pass-throughs in 120 high-speed crashes, versus 5
  before, and every crash now registers)
- **Smoother ground:** the road near the camera is drawn at full horizontal detail again, so lane
  lines and crosswalks no longer break into chunky blocks
- **Rear windows on every car:** hatchbacks, wagons, SUVs, minivans and pickups now have dark rear
  glass; upright glass no longer shows noisy sky reflections

## v0.17
- **Saving fixed.** Saves were being written, but a checksum mismatch made the game reject them on the
  next boot, so CONTINUE never appeared. Saves now load correctly. The game also autosaves after
  every completed job (manual saves still aren't allowed during a job or a police chase)
- **ROM is 42% smaller** (24.8 MB down to 14.3 MB): cars are left/right symmetric, so only half of
  the view angles are stored and the rest are drawn mirrored; unused terrain height data removed
- **Faster:** about 13-16% higher frame rate (often 16-19 fps instead of 13-15), from cheaper ground
  drawing near the camera, fewer divisions in wall and sprite drawing, and a faster minimap
- Cars now have real windscreens and rear windows (dark tinted glass) instead of body-coloured ones

## v0.16
- Smoother car rotation: cars now have 64 view angles up close (was 32), at the two nearest
  camera elevations, so they turn smoothly instead of snapping
- 7 new car types (26 car sets in all): long-roof station wagon, tuner coupe with a big rear wing,
  long luxury sedan, minivan with roof rails, cab + cargo box truck, 70s land yacht with a vinyl
  roof, and an ambulance. They all appear in traffic and parked around the city
- Car collision sizes now come straight from each model's real dimensions

## v0.15
- Title screen artwork: an original slanted "sunset chrome" GTA wordmark with a heavy outline and
  drop shadow, and stylized menu labels (NEW GAME / CONTINUE / PRESS A), in place of the pixel font
- All cars rebuilt from scratch (`tools/cars3.py`) in a pre-rendered handheld style: boxy shapes
  typical of each car type and era, made with real-world sizes, sharp edges, high-contrast
  lighting with shadowing in tight spots, glossy reflections and a dark outline. The lineup:
  90s compact sedan, long full-size sedan (also the taxi and police car), boxy hatchback,
  80s wedge coupe, mid-engine exotic, classic long-hood muscle car with a hood scoop,
  boxy SUV, square-body pickup and a box van. All of them are original designs that look like
  real cars without copying any real model

## v0.14
- The title screen just says GTA, in the game's own pixel font
- Vehicle damage: crashes, rams, gunfire and blasts wear cars down. The car's condition shows as
  a bar under your health. Damaged cars smoke, then catch fire, then explode about 5 seconds later,
  leaving a burnt-out wreck that smoulders
- Explosions: a fireball and smoke cloud with a big boom. They set nearby cars burning (chain
  reactions), kill people close by and hurt you if you're on foot nearby
- If your own car blows up with you inside, you're thrown clear and hurt, but never killed:
  crashes still can't kill you. Get out when it starts burning (R)
- With nobody in your sights, you shoot the car in front of you. Shooting police cars raises
  your wanted level
- Marta's garage repairs your car if you stop in front of it ($100, or $300 if it's burning)
- Burning traffic stops and the driver bails out. Wrecked traffic is replaced once you're far away
- Missions: wrecking the supercar or the delivery van fails the job. Blowing up Shaw's SUV
  also finishes him

## v0.13
- Renamed to **Gameboy Theft Auto**. The title screen has a city flyover, NEW GAME / CONTINUE
- Story: Eddie Vance comes to Steel Harbor to work for his cousin Rudy, finds him gone, and
  inherits a $10,000 "debt" to the loan shark Victor Moran. Marta (garage owner) and Jinx
  (street racer) help him, and Detective Shaw is Moran's man on the force. Each job opens with
  story dialogue (A to advance), and there's an ending
- 9 missions (was 4): Hot Wheels, Street Race, The Bookkeeper, Heat, Special Delivery (timed
  van run to the coast), The Ledger (lose the cops, reach the safehouse), Cash Run (ram Shaw's SUV
  until he bails, then stop him), Coast to Coast (timed checkpoint run round the island),
  Last Call (finale at the old pier)
- Pause menu (START): resume, save game, radio on/off, quit to title. SELECT+START still toggles
  the fps counter
- Save system: 32 KB battery SRAM (the ROM carries the SRAM_V113 tag, so emulators and flash carts
  give it a save). You can't save during a job or while wanted. CONTINUE puts you back on foot
  where you saved, with your money, guns and story progress
- Crashing no longer hurts you; only gunfire and being hit by cars on foot do
- Carjacking: stealing a car out of traffic pulls the driver out, and they run off shouting

## v0.12
- Much faster: about 13-16 fps in town in the cycle-counting profiler, vs 10-14 for the v0.6 build
  ("steelharbor4"). To get there: the ground is flat again (the hills in v0.11 were what made it
  crawl), the sound mixer moved to fast RAM, HUD text is drawn with hardware sprites, the radar uses a
  prebuilt island map, the sky only draws where it shows, and game logic runs while the previous frame
  waits for its screen flip. The curvy coastal road, beaches and coast traffic stay
- Car models rebuilt: rounded body sections (soft shoulders, tucked rockers, rounded corners), curved
  greenhouses, glass with depth and reflection streaks, larger wheels, bigger and clearer lights and grilles
- Two new guns: shotgun (spread that can drop a small group at close range) and rifle (long range,
  hits hard). 24 pickups around the city instead of 10. The pistol, shotgun and rifle are semi-auto
  (one shot per press of A), the SMG is fully automatic. L cycles through the guns you have ammo for
- New gunfire sound for each weapon (sharp crack, body, low boom and a room tail). Cops' shots get
  quieter with distance
- `tools/gbaprof.c`: a small headless GBA interpreter with rough cycle counts, used to measure fps
  and profile ROMs

## v0.11
- Terrain: the island now has real elevation. The city sits on gentle rolling hills (streets
  rise and dip, building lots are levelled pads), and around it there is a ring of wooded hills
- A winding coastal road loops the whole island. It climbs and falls along the shore with
  beaches and sea on one side and hillside on the other, and has dashed centre lines, edge
  lines, gravel shoulders and street lights. Connector roads link it to the city
  (roads 2 and 6 on every side)
- Traffic also drives the coastal loop when you're near the edge of town
- Cars feel the hills: gravity pulls along the slope (parked cars hold still). The camera
  follows the ground and pulls in when a bank is behind you
- The sea is solid, so you get pushed back to shore. The radar shows the coast road, beaches,
  grass and water

## v0.10
- Car bodies rebuilt to read as conventional cars: crisper panels and creases, separate front
  and rear bumpers (chrome on the full-size sedan and pickup, black on the SUV and van, body
  colour with a seam on the rest), front plates, side mouldings, black wheel-arch trim on trucks,
  mirrors on the doors
- Each type has its own lighting: rectangular lamps with amber markers, projector lamps,
  twin round lamps, stacked truck lamps, square SUV lamps, slanted sports-car lenses, ovals,
  quad lamps; chrome bar / slat / honeycomb grilles; bar, split, vertical, round and wide taillights

## v0.9
- Car lineup grounded in real-world vehicle types (all original designs): compact sedan,
  hot hatch, full-size crew-cab pickup, big full-size sedan (also the police car and taxi),
  mid-size two-row SUV, front-engine sports car, compact sports coupe, pony car, delivery van

## v0.8
- Four facade styles: brick tenements (sash windows, lintels, sills), brownstones (tall narrow
  windows, stone lintels and sills), office towers (ribbon glazing with mullions and spandrels),
  modern concrete (wide windows); striped shop awnings; dark plinth at street level
- Clouds that scroll with the camera; deeper blue sky
- Street props: traffic lights with mast arms over the intersection, fire hydrants, trash cans,
  benches, street trees in sidewalk tree pits, wooden water towers on older rooftops
- Manhole covers and tree pits in the street texture

## v0.7
- New collision: every car is a box sized to its model. Car-to-car hits use a separating-axis
  test, push both cars apart, exchange an impulse and spin them when hit off-centre; walls
  bounce you at the right angle and building corners can't be clipped; people are tested
  against the real car outline
- New original everyday vehicles: compact sedan (your starting car, bright metallic blue with
  silver multi-spoke wheels and red calipers), pony car, hot hatch, full-size crew-cab pickup,
  delivery van

## v0.6
- Big performance pass: walls drawn as vertical runs per facade band with inverse depth
  interpolated (no per-column division); near street rows rendered at half resolution;
  collision, HUD and the building list in fast IWRAM; a faster divide routine; distant
  pedestrians and traffic updated at reduced rate; collisions only near the player; cheaper
  atan2; at most 64 sprites per frame
- Knocked-down people now lie on the ground properly (the pose was being rendered standing)
- BUSTED / WASTED screens

## v0.5
- Cars no longer look tilted: each car is pre-rendered from 4 camera heights (16/11/7/4 degrees)
  and the game picks the one that matches its real viewing angle
- New walk cycle (heel strike, knee lift, arm swing, feet locked to the ground) and a separate
  run cycle (forward lean, long stride, bent arms, airborne phase). Backward walking plays in reverse
- About 3x the pedestrians (42 on the street at once, kept around you as you move), 7 outfits,
  people run from gunfire; traffic doubled to 20 cars
- Police: patrolling beat cops and black-and-white patrol cars with flashing light bars and a siren.
  Crimes earn wanted stars (up to 5); more stars bring more cars. Cops ram you, jump out and
  chase on foot, and shoot from 2 stars up. At 1 star they try to arrest you. Stars fade if no cop
  sees you for a while
- Weapons: pistol and SMG with auto-aim, muzzle flash and gunshot sound. Pickups around the city
  for pistol, SMG and health. Health bar; you respawn at the hospital (KNOCKED OUT) or the police
  station (ARRESTED)
- Four jobs, started at yellow markers: steal a supercar and deliver it, a street race against
  two rivals through 7 checkpoints, a hit on a man guarded by armed bodyguards, and lose a
  3-star police chase. Money rewards
- HUD: rotating radar with job / checkpoint / police blips, money, wanted stars, health,
  weapon and ammo, job text, race checkpoint counter and countdown

## What's new in v0.4
- All cars rebuilt as realistic original designs, lofted from real side, plan and
  cross-section profiles: GT coupé (your starting car), mid-engine supercar, muscle car,
  sport compact, plus everyday sedans, a taxi and SUVs
- Greenhouses with A/B/C pillars and window trim, clear-coat paint with horizon
  reflections, LED headlights and taillights, grilles, shut lines, door handles, mirrors,
  multi-spoke wheels with brake discs and calipers, wheel-arch shading, anti-aliased edges

## v0.3
- Natural-looking people: anatomically shaped bodies (tapered limbs, shaped torso,
  shoulders, calves), faces with jaw, nose, ears, eyes, brows and mouth, real hairstyles
  (short and long), clothing folds, and a smoother walk cycle with knee bend, hip sway and arm swing.
  Five outfits (the player plus four pedestrian types)
- People and cars are drawn at the GBA's full 240-pixel horizontal resolution (the city
  itself still uses double-wide pixels for speed)
- Pedestrians hit by a car above walking pace are knocked down: blood spray particles,
  a pool that spreads over a few seconds, a knocked-down pose and a thud sound. Bodies clear after about 30 s
- Original street-racing cars: sports coupé with a wing and side vinyl, muscle car with
  hood scoop and racing stripes, hot hatch with a body kit, wing and flame vinyl. Low-profile tyres and spoked rims
- Street-racing handling: the car keeps its own velocity and the tyres grip it back toward
  the heading, so it slides; handbrake drifts; nitro boost (top speed about 140 mph → 180 mph);
  the camera follows your slide and pulls back with speed

## v0.2
- Pre-rendered sprites of original 3D models, ray-marched with reflections, specular
  highlights and soft shadows: sedans, a taxi and SUVs in several paint colours (64 views each),
  the player and three pedestrian outfits with 8-direction walk cycles, trees and street lamps
- Buildings with floors, window bays with varied glass reflections, sills, storefronts,
  a band above the ground floor and cornices
- Textured streets (patched asphalt, worn tyre tracks, sidewalk slabs, curbs, crosswalks)
- Distance haze with dithered fog (4 palette levels blended so they look like 16)
- Traffic, about 25 parked cars (all enterable), pedestrians walking the sidewalks
- Sound: synthesised engine with a 4-speed RPM model, horn, tyre squeal, crashes,
  doors and footsteps (DirectSound), plus an original radio loop on the PSG channels

## Rebuilding the art
`tools/sprites_sdf.py` holds the models. Change shapes or colours there, then run
`REGEN_ASSETS=1 ./build.sh` (about 1 minute).

## Engine notes
- Mode 4 page-flipped, rendered at 120x160 with double-wide pixels
- Building walls are drawn column by column with a per-column clip, so there is no overdraw.
  The window texture is perspective-correct, with one BIOS divide per column
- The ground is floor-cast row by row (near rows at half resolution) into every pixel no wall covers.
  Outside the street grid it reads the island material map from `tools/gen_terrain.py`. Lane lines
  on the curved coast road come from an interpolated lateral road coordinate, so they stay crisp
  at any angle
- Sprites are scaled per column, sorted far to near, and clipped against each column's nearest wall
  and the ground in front of them
- The renderer is ARM code in IWRAM; game logic is Thumb in ROM

## Building
Needs clang, ld.lld, llvm-objcopy and python3 (no devkitPro):

    LOGO_ROM=/path/to/any_gba_rom_you_own.gba ./build.sh

`LOGO_ROM` copies the boot logo into the header, which real hardware needs. mGBA boots without it.
The PC preview (writes frames as images): see `src/host_main.c`.
