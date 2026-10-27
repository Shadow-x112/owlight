# Scenarios

Scripted runs of the sim: which slimes there are, and what the player does, day by day. Each file
replays exactly (same inputs, same outcome) and checks itself with `expect` actions.

```
./build/sim/owlight_sim --scenario scenarios/first_day.json
./build/sim/owlight_sim --scenario scenarios/first_day.json --data data --hidden
```

The CLI prints one journal for the whole household, what you'd notice about each slime at the
end, and the expectations; it exits 1 if any failed. Every `*.json` here is also a `ctest` test
(built-in catalog and `data/` catalog), so adding a file adds a test.

`first_day.json` is `docs/design.md` §8.

## Format

```json
{
  "note": "what this scenario shows",
  "days": 3,
  "slimes": [ { "id": "inky", "seed": 1, "habitat": "pen" } ],
  "script": [
    { "day": 1, "actions": [
      { "at": "08:00", "slime": "inky", "feed": "charcoal" },
      { "at": "16:00", "slime": "inky", "expect": { "type": "plain", "sign": "cloudy" } }
    ]}
  ]
}
```

- `slimes` — the starting household. `seed` gives the small starting variance (0 = exactly at
  rest, default); `habitat` is where it lives.
- `script` — days in order, starting at 1; actions within a day in time order (`"at"`, `"HH:MM"`).
  Actions at the same time run in file order. Day is 06:00–20:00.
- `days` — optional; the run lasts to the end of this day. Default: the last scripted day.
- Any slime, day or action may carry a `"note"` (ignored).

Each action has `"at"`, `"slime"` (except `find`) and exactly one verb:

| Verb | Value | Does |
|------|-------|------|
| `feed` | food id | one feeding |
| `move` | habitat id | it lives there from now on |
| `assign` | job id, or `null` | sets its job (`null`: nothing to do) |
| `pick_up` | `true` | picked up by hand |
| `name` | text | the player names it |
| `near` | `true` / `false` | whether the player spends the daytime close to it |
| `find` | new slime id | a slime turns up; also takes `habitat` (required) and `seed` |
| `expect` | `{ "type", "locked", "sign", "journal" }` | checks: current type id; locked in or not; some sign the player would notice right now contains this text; some journal entry so far contains this text |

Unknown keys, foods, habitats, jobs, types or slimes, bad times and time going backwards are
errors naming the file and line. Times in the output are world time (day 1 00:00 = the start);
each slime's own journal counts from when it appeared.
