# Owlight — working notes for Claude sessions

Owlight is a cozy isekai slime-research life sim. Read `docs/design.md` first; it is the fixed
base and the source of design decisions. Do not re-litigate settled decisions in code comments or
PRs — propose changes in `docs/design.md` §11 instead.

## Layout

- `docs/design.md` — the design document (settled decisions, v1 scope, build order).
- `sim/` — the simulation core: standalone C++17, **no engine dependency, no third-party deps**.
  - `sim/include/owlight/` — public headers.
  - `sim/src/` — implementation. `CatalogLoad.cpp` + `Json.*` parse the catalog; `Catalog.cpp`
    holds no numbers. `Catalog::v1()` parses `data/*.json` embedded at build time
    (`EmbeddedData.cpp.in`), so the shipped catalog needs no files on disk.
  - `sim/tools/owlight_sim.cpp` — CLI harness for running scenarios ("feed charcoal for 7 days").
  - `sim/tests/` — tests, run with `ctest`.
- `data/` — **the only copy of the numbers**: foods, habitats, jobs, types as JSON. Format and
  scale notes in `data/README.md`. Tune here; a plain rebuild re-embeds it.
- Unreal project lives on the owner's PC, not in this repo (yet).

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/sim/owlight_sim --days 7 --feed charcoal
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
