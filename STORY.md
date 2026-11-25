# GTA (Gameboy Theft Auto): New York — story bible (v0.33)

The plan for a story of about **6 hours**: 4 acts, 40 story missions, plus side content. Each act opens a new district of the map built in v0.30 (see `ROADMAP.md`).

**All four acts are in the game (v0.33)**: 40 story missions. Acts Two to Four run on the scripted mission engine (`src/story_acts.h`).

## Premise
**Nick Melnyk** (born Mykyta; everyone has called him Nick since basic training) is a sergeant in the Ukrainian army, ten years in, calm under fire and quiet about what he has seen. His uncle **Bohdan**, who raised him for a while as a boy, emigrated to New York twenty years ago and built a car shop on the waterfront, *Bohdan's Auto*. When Bohdan stops answering his phone, Nick takes leave, gets on a plane and comes to find him.

He finds the shop on the **New York waterfront**: docks, tugboats, rowhouses and a lot of people who owe the wrong man money. Bohdan is in hospital with both arms broken. Loan shark **Victor Moran** has a page from his ledger saying Bohdan owes him $10,000, with the shop as collateral. Bohdan's foreman **Marta** is keeping the doors open, and the shop has a side business in cars nobody asks about.

Bohdan never borrowed a dime, and his best friend **Sal**, a tug captain, died a few months ago when his boat, the *Lucky Marie*, sank in a storm. The police called it an accident. But Moran owns half the docks and has a detective on his payroll, so Nick starts paying.

Theme: **debts, who invents them, and who pays.** New York runs on favours, and Nick, a stranger in the city, learns to trade them.

## Cast (all original)
| Character | Who they are | Outfit in game | Acts |
|---|---|---|---|
| **Nick Melnyk** | The player. Ukrainian army sergeant, new to New York. Calm, dry, fiercely loyal to Bohdan; handy with a gun, keeps the war to himself, learns the city one job at a time. | Player model | 1-4 |
| **Bohdan Melnyk** | Nick's uncle. Owns Bohdan's Auto on the waterfront; gruff, generous, too proud to ask for help. Out of hospital by Act Two, with opinions about everything Nick does. | (new: older man) | 1-4 |
| **Sal** | Bohdan's best friend, a tug captain (dead). Refused to run Moran's cargo on his tug. | — | (memory) |
| **Marta Kowal** | Foreman at Bohdan's Auto under the elevated rail, and runs its stolen-car side business. Tough, practical. First to give Nick work. | Red | 1-4 |
| **Jinx** | Courier who moves anything for anyone. Funny, nervous, always three moves ahead or behind. | Green | 1-3 |
| **Victor Moran** | Loan shark and docks boss. Invents debts to seize waterfront homes. | Suit | 1 |
| **Det. Ray Shaw** | Crooked detective, Moran's man on the force. | Navy | 1 |
| **Rudy Brandt** | Sal's deckhand on the *Lucky Marie*; survived the sinking and went into hiding. | White | 1-4 |
| **Councilman Edgar Pryce** | Charming city councilman. The *Harbor Point* casino and luxury-waterfront project needs every dock home emptied, and Moran's fake debts were how. The real villain. | Suit | 2-4 |
| **Dee Okafor** | Investigative reporter at the *Harbor Gazette*. Wants Moran's ledger to bring down Pryce. | (new) | 2-4 |
| **Tomas "King" Kane** | Leader of the Eastside Kings, whose cars Nick burned for Moran. Enemy first, ally later. | Grey | 2-4 |
| **Ingrid Halvorsen** | Head of Halvorsen Shipping, Pryce's partner. Runs a private security firm, **Blackline**. | White | 3-4 |
| **Cole Mercer** | Blackline's field commander, ex-military, Nick's physical match. | Cop (recoloured) | 3-4 |

## The four acts
| Act | Name | District (new on the map) | Missions | Time | The arc |
|---|---|---|---|---|---|
| 1 | **Debts** | Docks and Old Town (north-west) | 9 | ~1 h 15 | Nick pays a fake debt; learns Moran sank the *Lucky Marie*; kills Moran. |
| 2 | **Paper** | Downtown (north-east), across the river | 11 | ~1 h 30 | Moran was a tool. Nick and Dee chase the paper trail to Councilman Pryce while Moran's leftover crew and the Kings fight over the docks. |
| 3 | **Iron** | The Industrial port (south-east) and the Heights (south-west) | 11 | ~1 h 45 | Pryce brings in Halvorsen's Blackline. A betrayal costs Nick his safehouse and nearly Bohdan. |
| 4 | **Tide** | Gull Island (the half-built casino, far south-east) | 9 | ~1 h 30 | Nick unites everyone Pryce stepped on and takes the island apart. |

Side content (see the end) adds about 1-2 hours on top.

## Act One — Debts (in the game)
| # | Title | Giver | What you do | Reward |
|---|---|---|---|---|
| 1 | Hot Wheels | Marta | Steal the yellow supercar, bring it to the garage (fails if wrecked) | $500 |
| 2 | Candy Run | Jinx | Collect a package at the docks (1 star), deliver to the club within 100 s | $800 |
| 3 | The Bookkeeper | Moran | Kill Moran's bookkeeper, who is talking to the police; 3 bodyguards | $1,500 |
| 4 | Wrong Corner | Moran | Burn the Eastside Kings' 3 parked cars; 3 armed gang members | $1,800 |
| 5 | Dirty Blue | Shaw | Shaw threatens Bohdan in his hospital bed. Kill 2 honest officers at the old mill, then lose a 3-star wanted level | $2,200 |
| 6 | Special Delivery | Moran | Drive a van to the east coast road in 90 s (fails if wrecked) | $2,500 |
| 7 | The Ledger | Jinx | Jinx stole Moran's ledger: the debts are invented to take waterfront homes. Kill 3 gunmen, lose the heat, reach the safehouse | $3,000 |
| 8 | Cash Run | Marta | Stay on Shaw's SUV until he bails, then kill him | $3,500 |
| 9 | Last Call | Rudy | Rudy reveals Moran's men sank the *Lucky Marie* with Sal aboard. Kill Moran at the old pier | $10,000 |

**Ending of Act One:** Moran's phone keeps ringing with one name, *Pryce*. Rudy explains Councilman Pryce wants the whole waterfront for a casino. Nick: "Then this isn't over."

## Act Two — Paper (Downtown, in the game)
Moran is dead, but the houses are still being taken, now by "legal" eviction notices. Nick teams up with the reporter Dee to prove who is behind it, while Moran's nephew **Danny Moran** and the Eastside Kings fight over the docks.

| # | Title | Giver | What you do |
|---|---|---|---|
| 10 | Front Page | Dee | Pick Dee up downtown, shake the two cars tailing her (wreck or lose them), drive her to the Gazette. |
| 11 | Eviction Notice | Bohdan | Race to Pier Street before the truck leaves, drive off the hired muscle, wreck the eviction truck, lose the cops. |
| 12 | Crown Debt | Kane | Steal three cars from Danny's lot in the port for the Kings: the second brings the cops, the third brings Danny's crew. |
| 13 | City Hall Ink | Dee | Tail Pryce's aide Baxter without being seen, get out and listen in (Halvorsen Shipping), report back. |
| 14 | Photo Op | Dee | Photograph the marina meeting from three spots, then fight off Blackline security and lose the cops. |
| 15 | Danny Boy | Marta | Defend Bohdan's Auto from three waves of Danny's men, then chase Danny's SUV and wreck it. |
| 16 | Kings' Ransom | Kane | Ride the kidnappers' van until it stops, take them out, get Luis in your car and bring him home. |
| 17 | Shredder | Jinx | Stop the shredding truck, take it, lose the cops in it and deliver the bags to the Gazette. |
| 18 | Motorcade | Dee | Ride Baxter's car until he bails, stop him running, grab the briefcase, lose 3 stars, bring it to Dee. |
| 19 | Moran's Heir | Kane | Hit Danny's warehouse with the Kings (SMG provided), chase his supercar, finish him. |
| 20 | Retraction | Dee | The Gazette is firebombed: stop the arsonists, get Dee out, survive and lose 3 stars, reach the safehouse. Pryce's lawyers kill the story. |

## Act Three — Iron (the Industrial port and the Heights, in the game)
Pryce stops pretending. Halvorsen Shipping's private security firm, Blackline, moves into the port: armoured SUVs, checkpoints and better guns than the police. Pryce offers Nick a deal and is refused. Jinx sells Nick out to save his mother.

| # | Title | Giver | What you do |
|---|---|---|---|
| 21 | New Management | Rudy | Scout four Blackline checkpoints on foot (one patrol spots you), report back. |
| 22 | Container 7 | Marta | Stop a Halvorsen truck, take it, shake two Blackline SUVs, bring it to the shop: forged deeds for 200 homes. |
| 23 | The Offer | Pryce | Pryce offers Bohdan's deed if Nick leaves. Nick refuses; fight off the grounds, shake the SUVs. |
| 24 | Night Shift | Bohdan | Drive Bohdan home, wreck the two cars following you, clear the men waiting at his apartment. |
| 25 | Iron Yard | Kane | With a rocket launcher from the Kings, destroy three Blackline SUVs at the scrapyard. |
| 26 | Paper Trail | Dee | Stop the courier's car, take the hard drive, wreck Blackline's pursuit, deliver it to Dee's new office. |
| 27 | Hill Climb | Jinx | Drive Jinx's getaway car to Halvorsen's house. It's a trap: fight out, lose 3 stars. |
| 28 | Burned | Marta | The safehouse is raided: fight out, survive and lose 4 stars, reach Marta's backup garage. |
| 29 | Mercer | Marta | Storm the Blackline compound (rifle provided), free Rudy, shake Mercer's SUVs, bring Rudy to the docks. |
| 30 | What Jinx Knew | Jinx | Jinx confesses and tells you about the Gull Island meeting. **Choice:** shoot him, or walk up and let him go. |
| 31 | Drawbridge | Kane | Hold the bridge control house north of Gull Island through four waves while Marta's crew crosses. |

## Act Four — Tide (Gull Island, in the game)
Gull Island, the half-built *Harbor Point* casino, opens up. Everyone Pryce stepped on comes with Nick: the Kings, Marta's crew, Rudy, Dee with a camera, and Bohdan.

| # | Title | Giver | What you do |
|---|---|---|---|
| 32 | Landing Party | Rudy | Cross to the island, check two Blackline posts on foot, take out a patrol, reach the construction site. |
| 33 | Power Down | Marta | Destroy the three generator trucks that run the cameras, mop up the guards. |
| 34 | Hard Hats | Kane | Hold the back gate through three waves while the Kings hit the front, then clear the yard. |
| 35 | Live Feed | Dee | Pick up Dee, wreck the SUVs coming for her, defend the crane while she broadcasts the forged deeds live. |
| 36 | Mercer, Again | Mercer | Boss fight: armoured Cole Mercer and his guards at the casino. |
| 37 | Halvorsen Sails | Rudy | Chase Halvorsen's maroon land yacht of a car and wreck it, then her bodyguards. She is arrested. |
| 38 | Councilman | Dee | Fight through two floors of Pryce's guards to the helipad. |
| 39 | The Lucky Marie | Pryce | Pryce admits the *Lucky Marie* was sunk on his say-so, on camera. **Choice:** shoot him, or walk up and let the police take him. |
| 40 | Home | Bohdan | Drive Bohdan home to the shop. The end; free roam carries on. |

**Endings:** Pryce arrested (the "clean" ending: the story wins and he grows old in a cell) or Pryce shot (Nick's revenge, live on camera). Both close with "Sal can rest."

## Side content (about 1-2 hours)
- **Vehicle jobs:**
  - Taxi fares (taxi).
  - Ambulance runs for Harbor General (ambulance).
  - Police vigilante (police car).
  - Marta's car export lists (steal specific cars).
- **Tug jobs with Rudy** (after Act Two): tow barges, boat races around the harbor.
- **Hidden packages:** Sal's 50 lost tug parts around the city. Rewards come at milestones (weapons at home, armour, and the restored *Lucky Marie* at 50).
- **Stunt jumps**, a **stats screen**, and properties to buy (a share of Bohdan's Auto, the club, a dock warehouse) that pay a little income.
- **Kings turf fights** in Act Two and later: random Danny Moran crews attack Kings corners.

## How missions are built
- Act One uses the existing objective types: steal and deliver, collect and deliver, kill target(s), destroy cars, timed drive, chase until bail-out, lose the wanted level.
- Acts Two to Four are step lists (`src/story_acts.h`) run by the engine in `game.c`. Step types: go somewhere (on foot, in any car, or in the job's car; optionally with a timer or "lose the cops first"), get in a car, deliver it, kill a group (or a fleeing target), wreck parked cars, chase a car (wreck it, or ride its bumper until the driver bails), tail a car unseen, defend a spot against waves, hunter cars that ram you (wreck or shake them), lose a wanted level, survive a wanted level for a time, photograph a spot, a mid-mission phone call, get a weapon, a boss, a choice (shoot or spare), and picking up a passenger.
- There are no boats yet, so the boat chases from the first plan became car chases.
- **Cutscenes:** in-engine, with letterbox bars and a dialogue box. The mission giver stands in the street facing Nick: an establishing two-shot first, then over-the-shoulder cuts to whoever is speaking. Afterwards the giver walks away. Portraits stay off unless the owner supplies art.
- **New people needed:**
  - Dee and Marta have their own women's models since v0.40. Still to do: an older-man model for Bohdan (grey outfit for now).
  - A Blackline guard (they use the uniformed cop model for now).

## Systems
- **Title screen:** a "GTA" wordmark with NEW GAME / CONTINUE.
- **Pause menu (START):** resume, save, radio, quit to title.
- **Saving:** battery SRAM, plus an autosave after every job. You can't save during a job or while wanted. Loading puts you back in the car you saved in, or on foot with no car.
- **A crash alone never kills the player**, but a car that blows up with you inside does (WASTED). A burning car gives you at least 4 s and you can bail out at any speed.
- **Car damage, from 0 to 1000:** smokes from 650, burns from 880 and then explodes. Gunfire, rams and blasts all count.
- **Repairs:** Bohdan's Auto charges $100, or $300 if the car is burning.
