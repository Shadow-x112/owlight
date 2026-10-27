#pragma once
// Scripted scenarios: a day-by-day list of things the player does to one slime, with
// expectations along the way. Used to replay designed moments (the first day in
// docs/design.md §8) exactly, from a file, and assert them in tests.
//
// File format (JSON), e.g. scenarios/first_day.json:
//
//   {
//     "name": "first day",
//     "seed": 0,                       // starter variance; 0 = the resting slime
//     "habitat": "pen",                // where it starts
//     "end": "day 2 09:00",            // when the scenario stops
//     "steps": [
//       { "at": "day 1 08:00", "feed": "charcoal" },
//       { "at": "day 1 14:00", "pick_up": true },
//       { "at": "day 1 21:00", "name": "Cinder" },
//       { "at": "day 1 21:00", "move": "stove_corner" },
//       { "at": "day 1 21:00", "assign": "scrub_stove" },   // or null to give it nothing to do
//       { "at": "day 1 09:00", "near": true },               // player stays close from now on
//       { "at": "day 2 08:00", "expect": { "type": "soot", "locked": false,
//                                           "sign": "sits heavy" } }
//     ]
//   }
//
// Times are "day D HH:MM" (day 1 starts at hour 0) or a plain number of hours. Steps run in
// time order; several steps may share a time and then run in file order. The sim is ticked in
// one-hour steps between them, with daytime 06:00–20:00.

#include "owlight/Catalog.h"
#include "owlight/Slime.h"

#include <optional>
#include <string>
#include <vector>

namespace owlight {

struct Expectation {
    std::optional<std::string> type;
    std::optional<bool> locked;
    std::optional<std::string> sign;   // a visible sign's text must contain this
};

struct Step {
    double atHours = 0.0;
    int line = 0;                       // in the scenario file, for messages
    enum class Kind { Feed, PickUp, Name, Move, Assign, Near, Expect } kind;
    std::string arg;                    // food / name / habitat / job id
    bool flag = false;                  // pick_up / near / assign-null
    Expectation expect;
};

struct Scenario {
    std::string name;
    unsigned seed = 0;
    std::string habitat = "pen";
    double endHours = 24.0;
    std::vector<Step> steps;            // sorted by time, stable

    // Parse from text or a file. Validates ids against `catalog`.
    static std::optional<Scenario> parse(const std::string& text, const Catalog& catalog, std::string* error);
    static std::optional<Scenario> loadFile(const std::string& path, const Catalog& catalog, std::string* error);
};

struct Failure {
    double atHours;
    int line;
    std::string message;
};

struct ScenarioResult {
    std::vector<Failure> failures;      // unmet expectations, in order
    bool ok() const { return failures.empty(); }
};

// Deterministic starter values for a seed. Seed 0 is the resting slime (all 0.5).
PropertyValues starterValues(unsigned seed);

// Run the scenario on a fresh slime (built with `starterValues(seed)` and `rates`). The slime
// is left in its final state in `out` so callers can print its journal and signs.
ScenarioResult runScenario(const Scenario& scenario, const Catalog& catalog, Slime& out);

// Convenience: construct the slime for a scenario.
Slime makeScenarioSlime(const Scenario& scenario, const Catalog& catalog, Rates rates = {});

} // namespace owlight
