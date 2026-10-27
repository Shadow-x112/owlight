# Owlight — handoff for Claude Code sessions

How this project is worked on: design and decisions happen in the owner's main Claude chat;
building happens in Claude Code cloud sessions against this repo (`Shadow-x112/owlight`, `main`).
The owner's Unreal work happens on his own PC, where this repo is cloned at `Documents\owlight`.

Each cloud session: read `CLAUDE.md`, then `docs/design.md`, then this file. Update the
**Status** and **Next** sections at the end of every session and commit them with the work.

## Status (2026-10-27)

- Design doc written and fixed: `docs/design.md`.
- Sim core done for v1 and green: `sim/` (C++17, no deps). 25 tests pass. `owlight_sim` CLI runs
  scenarios and prints only what the player would notice.
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
./build/sim/owlight_sim --list
```

## Next (in order)

1. **Catalog as data.** Move foods, habitats, jobs and types out of `sim/src/Catalog.cpp` into
   plain data files under `data/` (JSON, hand-written parser — no third-party deps) with
   `Catalog::load(path)`. Keep `Catalog::v1()` as the built-in fallback and add a test proving the
   loaded catalog equals the built-in one. The numbers must not change.
2. **Scenario files.** Let `owlight_sim` run a scripted scenario file (day-by-day actions: feed,
   move, assign, pick up, name) so the first day in `docs/design.md` §8 can be replayed exactly
   and asserted in a test.
3. **Save/load.** Serialize a `Slime` (values, exposure, type, lock, journal) so Unreal can
   persist it. Round-trip test.
4. **C API shim.** A thin `extern "C"` layer over `Slime`/`Catalog` so Unreal can call the core
   without pulling in STL types. Only after 1–3 are done.
5. Then the owner does the Unreal style proof on his PC (not a cloud task).

## Rules that are easy to forget

- The player never sees numbers. Any new player-facing text goes through `Journal` signs.
- Deterministic. No randomness inside the sim; variance comes from starter values only.
- `Slime::tick` integrates in ≤1h sub-steps; keep every new effect inside `Slime::step`.
- Every tuning change must keep `a_day_of_charcoal_makes_a_soot_slime_by_morning` passing.
- Don't start the Unreal layer in the cloud; Unreal isn't available there.

## Open design questions (owner decides, not the session)

World and town names; the gods' one-paragraph reason; villager roster; style-proof scene details.
