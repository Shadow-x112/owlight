# Owlight — working notes for Claude sessions

Owlight is a cozy isekai slime-research life sim. Read `docs/design.md` first; it is the fixed
base and the source of design decisions. Do not re-litigate settled decisions in code comments or
PRs — propose changes in `docs/design.md` §11 instead.

## Layout

- `docs/design.md` — the design document (settled decisions, v1 scope, build order).
- `sim/` — the simulation core: standalone C++17, **no engine dependency, no third-party deps**.
  - `sim/include/owlight/` — public headers.
  - `sim/src/` — implementation. `Catalog.cpp` holds the built-in v1 foods, habitats, jobs and type
    thresholds (`Catalog::v1()`); `CatalogLoad.cpp` + `Json.*` read the same catalog from `data/`.
  - `sim/tools/owlight_sim.cpp` — CLI harness for running scenarios ("feed charcoal for 7 days").
  - `sim/tests/` — tests, run with `ctest`.
- `data/` — the catalog as JSON (`Catalog::load("data")`), format in `data/README.md`. Must equal
  `Catalog.cpp` exactly; a test enforces it, so tune both together.
- `scenarios/` — scripted day-by-day runs with built-in `expect` checks (`owlight_sim --scenario`);
  each file is a ctest. `first_day.json` is `docs/design.md` §8. Format in `scenarios/README.md`.
- Unreal project lives on the owner's PC, not in this repo (yet).

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/sim/owlight_sim --days 7 --feed charcoal
./build/sim/owlight_sim --scenario scenarios/first_day.json
```

## Rules for the sim

- Properties are hidden `[0,1]` values; the player never sees numbers. Everything the player sees
  comes from `Journal` signs and the resolved `Type`.
- Deterministic: same inputs (and seed) → same outcome. No hidden randomness in nudges.
- Time is real hours; `Slime::tick(hours, ctx)` must be safe to call with large `hours`
  (player away for days) and with tiny steps alike — integrate, don't assume a step size.
- Keep the core free of Unreal types so it can be unit-tested here and wrapped later.

## Working style

- The owner likes each part brought close to finished before moving on. Prefer finishing the sim
  properly over sketching the next layer.
- Commit small, descriptive, to `main` unless asked for a branch.
