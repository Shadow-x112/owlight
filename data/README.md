# Catalog data

The v1 catalog as plain JSON. **These files are the only copy of the numbers.** `Catalog::load("data")`
reads them from disk; `Catalog::v1()` parses copies that CMake embeds at build time, so the shipped
game needs no files and can never drift from this directory. Edit here, rebuild, done. Design intent
for every entry is in `docs/design.md` §5–6.

## Scale

- Every property lives in `[0, 1]` and **rests at 0.5**. Drift pulls each value back toward 0.5 at
  1% of its distance per hour, so a value at 0.7 loses about 0.05 a day untended, and one at 0.9
  about 0.1 a day. Extremes fade faster than mild ones.
- Food deltas are **per feeding**; a slime is fed about three times a day. Charcoal's `clarity: -0.09`
  means three feedings pull clarity down by ~0.27 before drift.
- Habitat and job deltas are **per hour**; a slime spends ~24 h/day in a habitat and up to 24 h/day
  on a job. `0.004/h` is ~0.1 a day, enough to move a property a full band in a few days.
- Type thresholds are inclusive. The first-day test (`a_day_of_charcoal_makes_a_soot_slime_by_morning`)
  pins the feel: three charcoal feedings on day 1 are not enough; the morning feeding of day 2 is.

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
