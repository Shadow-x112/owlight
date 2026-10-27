# Owlight — handoff for Claude Code sessions

How this project is worked on: design and decisions happen in the owner's main Claude chat;
building happens in Claude Code cloud sessions against this repo (`Shadow-x112/owlight`, `main`).
The owner's Unreal work happens on his own PC, where this repo is cloned at `Documents\owlight`.

Each cloud session: read `CLAUDE.md`, then `docs/design.md`, then this file. Update the
**Status** and **Next** sections at the end of every session and commit them with the work.

## Status (2026-10-27)

- Design doc written and fixed: `docs/design.md`.
- Sim core done for v1 and green: `sim/` (C++17, no deps). 30 tests pass. `owlight_sim` CLI runs
  scenarios and prints only what the player would notice.
- **Catalog is data.** `data/foods.json`, `habitats.json`, `jobs.json`, `types.json` (format in
  `data/README.md`), read by `Catalog::load(dir)` through a small strict JSON parser
  (`sim/src/Json.*`, internal). `Catalog::v1()` stays as the built-in fallback.
  `loaded_catalog_equals_the_built_in_one` proves the two are equal field for field, exact doubles;
  `owlight_sim` output is byte-identical with and without `--data data`. Typos (unknown key,
  property, material), out-of-range values, duplicate ids and a missing `plain` are load errors
  naming `file:line`. Numbers parse with the classic locale, so a decimal-comma host can't skew them.
- A tuning change now goes in **both** `data/` and `Catalog.cpp`; the equality test catches drift.
- The first-day scenario behaves as designed: three charcoal feedings on day 1, "something is
  changing in it", cloudy by evening, *became a soot* on the morning feeding of day 2.
- Tuning lessons already encoded as tests: pearl needs the cold cave; a brook slime in a dry cave
  loses its water (the `cave_pool` habitat is the answer); signs use hysteresis so the journal
  doesn't flicker.

## Build and check

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build --output-on-failure
./build/sim/owlight_sim --days 7 --feed charcoal
./build/sim/owlight_sim --data data --days 7 --feed charcoal   # catalog from data/
./build/sim/owlight_sim --list
```

## Next (in order)

1. **One source of truth for the catalog.** The JSON in `data/` is the truth. Build
   `Catalog::v1()` from the same files embedded at build time (CMake reads `data/*.json` into
   string literals in a generated `.cpp`; parse them with the existing loader). Delete the
   hand-written tables in `Catalog.cpp`. `v1()` must still work with no files on disk, every
   existing test must still pass unchanged, and `owlight_sim` output must stay byte-identical.
   Then drop the "tune in both places" rule from `CLAUDE.md`, `data/README.md` and this file,
   and move the scale reminders (what 0.5 means, how fast drift pulls) into `data/README.md`.
2. **Scenario files.** Let `owlight_sim` run a scripted scenario file (day-by-day actions: feed,
   move, assign, pick up, name) so the first day in `docs/design.md` §8 can be replayed exactly
   and asserted in a test. The JSON reader in `sim/src/Json.*` is there to reuse; scenarios should
   take a `--data` catalog like the rest of the CLI.
3. **Save/load.** Serialize a `Slime` (values, exposure, type, lock, journal) so Unreal can
   persist it. Round-trip test. Reference foods/habitats/jobs/types by id, not pointer, so a save
   survives a catalog reload.
4. **C API shim.** A thin `extern "C"` layer over `Slime`/`Catalog` so Unreal can call the core
   without pulling in STL types (include `Catalog::load` so Unreal reads `data/`). Only after 1–3
   are done.
5. Then the owner does the Unreal style proof on his PC (not a cloud task).

## Rules that are easy to forget

- The player never sees numbers. Any new player-facing text goes through `Journal` signs.
- Deterministic. No randomness inside the sim; variance comes from starter values only.
- `Slime::tick` integrates in ≤1h sub-steps; keep every new effect inside `Slime::step`.
- Every tuning change must keep `a_day_of_charcoal_makes_a_soot_slime_by_morning` passing.
- Until item 1 lands: tune in `data/*.json` **and** `sim/src/Catalog.cpp` together;
  `loaded_catalog_equals_the_built_in_one` fails if they drift.
- Don't start the Unreal layer in the cloud; Unreal isn't available there.

## Open design questions (owner decides, not the session)

World and town names; the gods' one-paragraph reason; villager roster; style-proof scene details.
