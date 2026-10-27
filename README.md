# Owlight

A cozy isekai slime-research life sim. You are dropped into a quiet forest with three plain slimes.
Everything you feed them, everywhere you keep them, every job you give them nudges hidden properties
nobody can see. When enough cross a line together, the slime becomes something new, and you write it
down. The journal is the collection.

Design: [`docs/design.md`](docs/design.md). Notes for coding sessions: [`CLAUDE.md`](CLAUDE.md).

## The simulation core (`sim/`)

Standalone C++17, no engine, no dependencies. Unreal wraps it later.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

./build/sim/owlight_sim --days 7 --feed charcoal
./build/sim/owlight_sim --days 30 --feed river_stone --habitat cold_cave --hidden
./build/sim/owlight_sim --list
./build/sim/owlight_sim --scenario scenarios/first_day.json
```

`owlight_sim` runs one slime through a scenario and prints only what the player could have
noticed. `--hidden` shows the numbers, for tuning. `--scenario` replays a scripted file
(`scenarios/`, format in `sim/include/owlight/Scenario.h`) and exits 1 if any expectation in it
fails; `scenarios/first_day.json` is the first day from the design doc, pinned by a test.
