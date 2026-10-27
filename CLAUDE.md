# Owlight â€” working notes for Claude sessions

Owlight is a cozy isekai slime-research life sim. Read `docs/design.md` first; it is the fixed
base and the source of design decisions. Do not re-litigate settled decisions in code comments or
PRs â€” propose changes in `docs/design.md` Â§11 instead.

## Layout

- `docs/design.md` â€” the design document (settled decisions, v1 scope, build order).
- `sim/` â€” the simulation core: standalone C++17, **no engine dependency, no third-party deps**.
  - `sim/include/owlight/` â€” public headers.
  - `sim/src/` â€” implementation. `CatalogLoad.cpp` + `Json.*` parse the catalog; `Catalog.cpp`
    holds no numbers. `Catalog::v1()` parses `data/*.json` embedded at build time
    (`EmbeddedData.cpp.in`), so the shipped catalog needs no files on disk.
  - `sim/src/SlimeSave.cpp` â€” `Slime::save()`/`Slime::load()`: full state as JSON, by id, exact.
  - `sim/include/owlight/owlight_c.h` + `src/owlight_c.cpp` â€” the C interface Unreal calls.
    `tests/c_api_smoke.c` is compiled as plain C and runs the first day through it.
  - `sim/tools/owlight_sim.cpp` â€” CLI harness ("feed charcoal for 7 days", `--scenario FILE`,
    `--save`/`--load`).
- `scenarios/` â€” scripted scenarios (format in `sim/include/owlight/Scenario.h`). `first_day.json`
  is docs/design.md Â§8 and is pinned by a test; keep it passing.
  - `sim/tests/` â€” tests, run with `ctest`.
- `data/` â€” **the only copy of the numbers**: foods, habitats, jobs, types as JSON. Format and
  scale notes in `data/README.md`. Tune here; a plain rebuild re-embeds it.
- `unreal/` — the Unreal 5.8 side: project file, `Config/`, `Source/`, and the editor Python scripts`n  (style proof, asset import, slime build). `Content/` stays on the owner's PC (see `unreal/README.md`).

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
- Deterministic: same inputs (and seed) â†’ same outcome. No hidden randomness in nudges.
- Time is real hours; `Slime::tick(hours, ctx)` must be safe to call with large `hours`
  (player away for days) and with tiny steps alike â€” integrate, don't assume a step size.
- Keep the core free of Unreal types so it can be unit-tested here and wrapped later.

## Working style

- The owner likes each part brought close to finished before moving on. Prefer finishing the sim
  properly over sketching the next layer.
- Commit small, descriptive, to `main` unless asked for a branch.
