#pragma once
// Scripted scenarios: a day-by-day list of what the player does, replayed against the sim.
// This is how docs/design.md §8 (the first day) runs exactly, from a file, and is checked by a
// test. The file format is in scenarios/README.md.
//
// Replay is exact: time advances in whole-hour steps aligned to the clock, which is how the
// CLI and the tests have always driven a slime, so a scenario reproduces them number for number.

#include "owlight/Catalog.h"
#include "owlight/Slime.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owlight {

// Day runs 06:00–20:00 in every scenario, as in the CLI.
constexpr int kDayStartsAt = 6;
constexpr int kNightStartsAt = 20;

// What an `expect` action checks. Unset / empty fields are not checked.
struct Expectation {
    std::optional<std::string> type;   // the slime's current type id
    std::optional<bool> locked;
    std::string sign;                  // some sign the player would notice right now contains this
    std::string journal;               // some journal entry so far contains this
};

struct Action {
    enum class Verb { Feed, Move, Assign, PickUp, Name, Near, Find, Expect };

    Verb verb = Verb::Expect;
    double atHours = 0.0;              // world time: hours since day 1 00:00
    int line = 0;                      // where it is in the scenario file, for messages
    std::string slime;                 // scenario id it applies to; for Find, the new slime's id

    const Food* food = nullptr;        // Feed
    const Habitat* habitat = nullptr;  // Move, Find
    const Job* job = nullptr;          // Assign (null = given nothing to do)
    std::string text;                  // Name
    bool near = false;                 // Near: the player spends the daytime close to it
    unsigned seed = 0;                 // Find
    Expectation expect;                // Expect
};

struct StarterSlime {
    std::string id;
    unsigned seed = 0;
    const Habitat* habitat = nullptr;
};

// A parsed scenario. Its foods, habitats and jobs point into the catalog it was read against,
// so keep that catalog alive and run against the same one.
struct Scenario {
    std::string file;                  // for messages
    std::vector<StarterSlime> slimes;
    std::vector<Action> actions;       // in time order; ties keep file order
    int days = 1;                      // how long the run lasts

    static std::optional<Scenario> load(const std::string& path, const Catalog& catalog,
                                        std::string* error = nullptr);
    static std::optional<Scenario> parse(std::string_view text, const std::string& file,
                                         const Catalog& catalog, std::string* error = nullptr);
};

struct ScenarioSlime {
    std::string id;
    double bornAt = 0.0;               // world time it appeared; journal times are its own age
    Slime slime;
    const Habitat* habitat = nullptr;
    const Job* job = nullptr;
    bool near = false;
};

struct ScenarioRun {
    std::vector<ScenarioSlime> slimes;     // in order of appearance
    std::vector<std::string> failures;     // one line per failed expectation
    int expectations = 0;

    const ScenarioSlime* find(std::string_view id) const;
    bool passed() const { return failures.empty(); }
};

ScenarioRun runScenario(const Scenario& scenario, const Catalog& catalog, Rates rates = {});

} // namespace owlight
