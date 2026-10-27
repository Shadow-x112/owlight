# Catalog data

The v1 catalog as plain JSON, read by `owlight::Catalog::load("data")`. Same numbers as the
built-in `Catalog::v1()` in `sim/src/Catalog.cpp`; the test `loaded_catalog_equals_the_built_in_one`
fails if the two disagree, so a tuning change goes in both. Design intent for every entry is in
`docs/design.md` §5–6.

Each file is an array of entries with a unique `"id"`. Any entry may carry a `"note"` (ignored by the
loader; JSON has no comments). Unknown keys, properties and materials are errors that name the
file and line — a typo never becomes a silent zero.

Properties: `density heat acidity clarity appetite boldness memory affinity light bond`.
Materials: `none metal wood cloth water stone`.

Values live in `[0,1]` and rest at 0.5. Deltas list only the properties they move; the rest are 0.

| File | Keys |
|------|------|
| `foods.json` | `per_feeding` (deltas, one feeding — about 3 a day), `material` (default `none`), `exposure_hours` (how much being around that material one feeding counts as, default 0) |
| `habitats.json` | `per_hour` (deltas), `material` (default `none`), `daylight` (0 = never, 1 = fully open; default 1), `loud` (default false) |
| `jobs.json` | `per_hour` (deltas, per hour worked), `material` (default `none`) |
| `types.json` | `conditions` (`{ "density": { "min": 0.70 }, "acidity": { "min": 0.55, "max": 0.80 } }`, bounds inclusive, default 0 and 1), `material` + `min_affinity` (the slime's affinity must be this material, this strong), `lock_after_hours` (0 = never locks), `rare` |

`types.json` must contain `plain`. Types are tried most specific first (conditions, plus one for a
material); among equally specific types, file order decides.
