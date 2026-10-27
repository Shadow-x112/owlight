# Owlight — Design Document

*Owlight* (an old English word for dusk, "owl-light") is a cozy isekai slime-research life sim.
You are dropped into a quiet forest by gods who like you, with three plain slimes and a hut that
needs work. Everything you feed a slime, everywhere you keep it, every job you give it nudges hidden
properties nobody can see. When enough of them cross a line together, the slime becomes something
new — and you write it down. The journal is the collection.

Inspiration: the manga *By the Grace of the Gods* (slime research + slice-of-life), the loop of
*Minecraft* (survival base building, exploring) and *No Man's Sky* (collect, upgrade everything),
and the bones of *Animal Crossing* (real time, villagers, decorating, a museum).

This document is the fixed base. Decisions here are settled unless a later section says otherwise.

---

## 1. Pillars

1. **Research, not a Pokédex.** Slime types emerge from hidden rules the player discovers. The rules
   are fixed and consistent: feed iron in the cold and you *always* get the same thing, so players
   can tell each other. No visible stat numbers, ever.
2. **Quiet days.** Combat exists because the forest has things in it, but the game is about
   feeding slimes, fixing the hut, walking to the stream. Cozy first.
3. **Real time.** The world runs on the player's clock. Slimes change over real days. Seasons
   change what lives in the forest. "I fed it iron all week" is literally true.
4. **Hands, no tools.** Slimes are picked up by hand. No vacuum, no capture ball.
5. **The world is just a world.** A kingdom with towns, forests, guilds, old ruins. No tower on
   the horizon. The deepest thing in it is a cave you haven't found yet.

## 2. Isekai framing (kept light)

- **A favor, not a war.** The gods bring you over because they want someone to care for the slimes;
  nobody else bothers with them. That is your whole mandate and why you have the knack nobody else has.
- **The gods are an audience, not a plot.** They drop in now and then, delighted by what you've
  bred, occasionally asking for a specific slime. No rankings, no chosen ones, no rivals.
- Opening scene: short and warm. They've put you in a forest clearing with a hut and a note.
  The note is the tutorial. No pop-ups.

## 3. Camera, look, feel

- **Camera:** pulled-back third person with a slight tilt (Animal Crossing's camera). You see
  yourself, your slimes and the room at once.
- **Look:** *pixel art with realistic materials* — everything pixelated, but photoreal lighting and
  materials underneath: wet reflections, fog, light through leaves, subsurface glow in a slime.
  Rule: the world may use a slightly coarser pixel grid than the slimes, so the stars stay crisp.
- **Style proof first.** One Unreal scene: one slime on a forest floor, real light through leaves,
  the pixel post-process on top. If that shot looks right, everything follows.
- **Engine:** Unreal Engine 5, same pipeline as 21112. The simulation core (this repo, `sim/`) is a
  standalone C++ library with no engine dependency, wrapped by Blueprints later.

## 4. The core loop

1. **Catch** slimes in the wild. Rare ones live in odd places (deep caves, old ruins).
2. **Experiment**: diet, habitat, job, who it lives next to, how long. Types emerge.
3. **Put them to work**: a cleaner runs the house, scavengers bring back finds while you're away,
   a fire slime cooks, a bold one walks the forest with you.
4. **Grow a life**: a hut becomes a home becomes (v2) a shop in town. Villagers get used to you.
5. **Explore** for new slimes, new foods, new places to keep them. That is what pulls you deeper.

## 5. The hidden-rules model

Every slime carries ten hidden properties as values in `[0, 1]`. Nothing displays the numbers.
Everything the player does nudges them. Properties drift slowly back toward their resting point
when nothing is pushing them, so some paths reverse if you stop (and some lock in — see §5.3).

### 5.1 Properties

| # | Property | Raised by | Lowered by | Visible sign |
|---|----------|-----------|------------|--------------|
| 1 | **Density** | heavy foods (ore, stone, nuts) | sweet / soft foods | how it sits, how deep it sinks in the pen |
| 2 | **Heat** | fire, sun, spicy and dried foods | water, shade, night | faint glow (hot) or dew on the surface (cold) |
| 3 | **Acidity** | rotten food, sap, certain mushrooms | chalk, milk | what it does to what it sits on (grass yellows) |
| 4 | **Clarity** | clean water, minerals | mud, soot | translucency — the pixel-realistic payoff property |
| 5 | **Appetite** | being fed often | starving | size, how fast it chases food |
| 6 | **Boldness** | being near you, combat, loud places | isolation | the distance it keeps from you |
| 7 | **Memory** | repeating the same job | variety | finishing tasks without being told |
| 8 | **Affinity** | the material it has been around longest (metal, wood, cloth, water, stone) | — | what it drifts toward in a room |
| 9 | **Light** | hours of daylight | hours of darkness | when it is active |
| 10 | **Bond** | being picked up, named, kept close | neglect | it follows you |

Affinity is categorical-plus-strength (which material, how strongly). Bond does not gate types;
it gates loyalty (following, coming when called, v2 breeding).

### 5.2 Types are thresholds

A type is a set of property thresholds that must all hold at once. When they do, the slime visibly
changes (color, texture, size, a new behavior) and gains a job it can do. Examples (full v1 list in
§6): high Density + high Heat + metal Affinity → forge slime; high Clarity + low Light + water
Affinity → lantern slime that lights caves.

More specific types win over less specific ones. A slime that no longer meets its type's thresholds
reverts after a grace period — *unless* the type is marked locked-in.

### 5.3 Rules of thumb (v1)

- Every property has a **cheap, obvious nudge** findable on day one, and **one rare nudge** hidden
  in the world.
- Rare types need **two unlikely things at once**, and that is where exploration feeds in: the only
  cold-acid habitat is a cave you haven't found.
- **The slime tells you it's changing before it changes.** Texture shifts, it eats differently, it
  starts avoiding sunlight. Reading those signs is the research. The journal logs them.
- Players **name types themselves**. The journal records what they did; over time the notes become
  a working theory of the rules.
- **Consistency over surprise.** Same inputs, same outcome, every save, every player. A small
  per-slime variance in starting values makes individuals differ without breaking the rules.

## 6. v1 type list (subject to tuning, not to redesign)

Thresholds are listed in `data/types.json` (mirrored in `sim/src/Catalog.cpp`) and are the source of truth; this table describes intent.

| Type | Key conditions | What it does |
|------|----------------|--------------|
| Plain | none | the starting state |
| Soot | high density, low clarity, some acidity (charcoal diet) | cleans ash, soot, grime — the first-morning payoff |
| Brook | high clarity, water affinity, cool | keeps water clean; a bowl of it never fouls |
| Lantern | very high clarity, low light, water affinity | glows; lights caves and night paths |
| Forge | high heat, high density, metal affinity | holds heat; cooks, warms the hut, softens metal |
| Frost | very low heat, high density | keeps food cold; chills a room |
| Loam | mid acidity, wood affinity, decent density | garden slime: compost, soil, faster growth |
| Moss | low light, wood affinity, low clarity | mossy coat; quiet, hides, finds mushrooms |
| Scrub | high acidity, high memory | cleaner that strips stains; dangerous to cloth |
| Puff | very low density, high boldness | bounces ahead of you on walks; scout |
| Shade | very low boldness, low light | hides; near-invisible at night; finds lost things |
| Glutton | very high appetite | eats anything; disposal; grows large |
| Steady | very high memory (any job long enough) | the reliable worker; finishes tasks unprompted |
| Pearl (rare) | very high clarity, high density, cold, stone affinity | the museum piece; needs the cold cave |

## 7. Animal Crossing layer

- **Real time** (§1). Slime changes are measured in real hours and days.
- **Villagers**: 4–5 AI townsfolk in v1 with personalities, who comment on your slimes and move
  away if the town gets boring. (Agent tech shared with world-zero.)
- **Decorating**: home, pens, (v2) shop. Decor is an experiment input: a dark cluttered room pushes
  a slime differently than a sunny one.
- **Museum**: the journal becomes a public slime exhibit in town. The curator asks for things you
  haven't found.
- **Daily rituals**: mail, a shop with rotating stock, a wandering merchant with strange foods,
  (v2) a seasonal festival where people show their slimes.

## 8. The first day (the design test)

1. **Arrive.** Short god scene. Clearing, hut, note.
2. **Three slimes** in a pen. Plain, slightly different hidden values. One is a little bluer, one
   sits closer to the water.
3. **The first experiment is accidental.** The hut has dried grain, a mushroom, charcoal. One eats
   the charcoal and turns a shade darker. Nothing else happens. The hook: you noticed.
4. **Afternoon = the loop in miniature.** Fix one thing on the hut. Find a fourth slime under a
   rock by the stream and pick it up by hand. A villager wanders by; that is how you learn there
   is a town, which you cannot see from the hut.
5. **Night.** Slimes sleep where you put them. The journal's first page writes itself.
6. **Next morning = the payoff.** The charcoal slime is darker, denser, and has cleaned the ash out
   of the stove on its own. First working slime, under an hour of play, from something you did.

## 9. v1 scope (the version that proves the game)

- One forest region: clearing, stream, cave, hidden town.
- One season (autumn — best light for the look).
- 3 starter slimes, ~12 discoverable types (§6) plus one rare.
- Home: hut you repair, one pen, basic decorating.
- Town: 4–5 villagers, one shop, the museum.
- Gods: the opening scene and two or three drop-ins.
- **Not in v1:** the player's own shop, festivals, breeding, other seasons, other regions.

## 10. Build order

1. **Sim core** (this repo, `sim/`): properties, nudges, thresholds, real-time ticking, journal.
   Standalone C++17, CLI harness, tests. *Done.*
2. **Data**: foods, habitats, jobs, types in `data/*.json`, the only copy of the numbers. *Done.*
3. **Scenarios**: scripted day-by-day files (`scenarios/`); the first day (§8) replays exactly
   and is pinned by a test. *Done.*
4. **Save/load**: a slime's full state as JSON, exact to the bit; survives catalog changes. *Done.*
5. **C API** (`sim/include/owlight/owlight_c.h`): opaque handles, ids as strings, no C++ types
   across the boundary; proven from plain C by a test. *Done.* The core is complete for v1.
6. **Style proof** in Unreal: one slime, one forest floor, the pixel post-process. *Done* (2026-10-27;
   scripts and findings in `unreal/README.md`). 5 px world grid / 2 px slime grid; scanned Poly Haven
   forest under Hornbeams at a low dusk sun; slime = translucent sculpted dome + inner core, driven by
   Clarity / Heat / Density / Glow.
7. **Unreal project** ← *current*: third-person character, pens, hut, sim library wrapped in Blueprints.
8. The first day, playable end to end.
9. Town, villagers, museum.

## 11. Open decisions

- The world's name and the town's name.
- The gods: who sent you and why, in a paragraph.
- Villager roster.
- Exact numeric tuning of nudges and thresholds (lives in code; tune by simulation).

## 12. Names rejected

*Gloam* (existing Steam game, studio, and registered trademark), *Evenfall* (existing games),
*Dusking* (shadowed by DUSK / Duskers). *Owlight* checked 2026-10-26: no Steam title, no USPTO
mark; one 48-hour itch jam entry and the unrelated horror game *Owl Lights* nearby. Do a proper
trademark search before a Steam page goes live.
