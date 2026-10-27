// The built-in v1 catalog. The data files in data/ hold the same numbers and are what the game
// loads; the test `loaded_catalog_equals_the_built_in_one` fails if the two disagree, so a tuning
// change goes in both places. The design intent for each entry is in docs/design.md §5–6.
//
// Scale reminder: properties live in [0,1] and rest at 0.5. Food deltas are per feeding
// (a slime is fed ~3 times a day). Habitat/job deltas are per hour. Drift pulls every value
// back toward 0.5 at 1%/hour of its distance, so a value at 0.7 loses ~0.05/day untended.

#include "owlight/Catalog.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace owlight {

namespace {

using P = Property;
using M = Material;

Deltas deltas(std::initializer_list<std::pair<Property, double>> list) {
    Deltas d{};
    for (const auto& [p, v] : list) at(d, p) = v;
    return d;
}

Food mkFood(const char* id, Deltas d, M material = M::None, double exposure = 0.0) {
    return Food{id, d, material, exposure};
}

Habitat mkHabitat(const char* id, Deltas d, M material, double daylight, bool loud = false) {
    return Habitat{id, d, material, daylight, loud};
}

Job mkJob(const char* id, Deltas d, M material = M::None) {
    return Job{id, d, material};
}

Condition atLeast(P p, double v) { return Condition{p, v, 1.0}; }
Condition atMost(P p, double v)  { return Condition{p, 0.0, v}; }
Condition between(P p, double lo, double hi) { return Condition{p, lo, hi}; }

} // namespace

const Catalog& Catalog::v1() {
    static const Catalog instance = [] {
        Catalog c;
        c.fillBuiltIn();
        c.finish();
        return c;
    }();
    return instance;
}

void Catalog::fillBuiltIn() {
    // --- Foods (per feeding) -------------------------------------------------------------
    foods_ = {
        mkFood("grain",        deltas({{P::Density, 0.02}, {P::Heat, 0.01}})),
        mkFood("mushroom",     deltas({{P::Acidity, 0.04}, {P::Light, -0.02}, {P::Clarity, -0.02}})),
        mkFood("charcoal",     deltas({{P::Density, 0.06}, {P::Clarity, -0.09}, {P::Acidity, 0.03}, {P::Heat, 0.01}})),
        mkFood("berry",        deltas({{P::Density, -0.04}, {P::Clarity, 0.02}, {P::Acidity, 0.01}})),
        mkFood("honey",        deltas({{P::Density, -0.05}, {P::Heat, 0.01}, {P::Boldness, 0.01}})),
        mkFood("iron_scrap",   deltas({{P::Density, 0.08}, {P::Clarity, 0.02}}), M::Metal, 2.0),
        mkFood("river_stone",  deltas({{P::Density, 0.07}, {P::Clarity, 0.04}, {P::Heat, -0.01}}), M::Stone, 2.0),
        mkFood("spring_water", deltas({{P::Clarity, 0.07}, {P::Heat, -0.03}, {P::Acidity, -0.02}}), M::Water, 1.0),
        mkFood("mud",          deltas({{P::Clarity, -0.07}, {P::Density, 0.02}})),
        mkFood("rotten_fruit", deltas({{P::Acidity, 0.08}, {P::Clarity, -0.03}, {P::Density, -0.02}})),
        mkFood("milk",         deltas({{P::Acidity, -0.06}, {P::Clarity, 0.01}, {P::Density, -0.01}})),
        mkFood("chalk",        deltas({{P::Acidity, -0.08}, {P::Density, 0.03}, {P::Clarity, -0.02}}), M::Stone, 1.0),
        mkFood("dried_pepper", deltas({{P::Heat, 0.08}, {P::Acidity, 0.02}, {P::Density, -0.01}})),
        mkFood("sap",          deltas({{P::Acidity, 0.05}, {P::Density, 0.02}}), M::Wood, 2.0),
        mkFood("cave_moss",    deltas({{P::Light, -0.04}, {P::Clarity, -0.03}, {P::Acidity, 0.02}, {P::Heat, -0.02}})),
    };

    // --- Habitats (per hour) ------------------------------------------------------------
    habitats_ = {
        mkHabitat("pen",          deltas({}), M::None, 1.0),
        mkHabitat("stove_corner", deltas({{P::Heat, 0.004}, {P::Clarity, -0.001}}), M::Metal, 0.6),
        mkHabitat("stream_bank",  deltas({{P::Clarity, 0.003}, {P::Heat, -0.002}}), M::Water, 1.0),
        mkHabitat("cave",         deltas({{P::Heat, -0.003}, {P::Clarity, 0.001}}), M::Stone, 0.0),
        mkHabitat("cold_cave",    deltas({{P::Heat, -0.006}, {P::Clarity, 0.002}}), M::Stone, 0.0),   // the rare one
        mkHabitat("cave_pool",    deltas({{P::Heat, -0.003}, {P::Clarity, 0.003}}), M::Water, 0.0),   // dark water: lanterns
        mkHabitat("garden_bed",   deltas({{P::Acidity, 0.001}, {P::Density, 0.001}}), M::Wood, 1.0),
        mkHabitat("woodpile",     deltas({{P::Light, -0.001}}), M::Wood, 0.5),
        mkHabitat("sunny_sill",   deltas({{P::Heat, 0.003}}), M::None, 1.0),
        mkHabitat("cupboard",     deltas({}), M::Cloth, 0.0),
        mkHabitat("workshop",     deltas({{P::Heat, 0.001}}), M::Metal, 0.5, /*loud=*/true),
    };

    // --- Jobs (per hour worked) ----------------------------------------------------------
    jobs_ = {
        mkJob("clean_house",  deltas({{P::Acidity, 0.002}}), M::Cloth),
        mkJob("scrub_stove",  deltas({{P::Acidity, 0.002}, {P::Density, 0.001}, {P::Clarity, -0.002}}), M::Metal),
        mkJob("tend_garden",  deltas({{P::Acidity, 0.001}}), M::Wood),
        mkJob("carry_water",  deltas({{P::Clarity, 0.002}}), M::Water),
        mkJob("forge_assist", deltas({{P::Heat, 0.004}, {P::Density, 0.001}}), M::Metal),
        mkJob("scout",        deltas({{P::Boldness, 0.003}})),
        mkJob("fetch",        deltas({{P::Boldness, 0.002}})),
    };

    // --- Types ------------------------------------------------------------------------------
    // Thresholds are inclusive. `material` requires the slime's affinity material to match
    // with at least `minAffinity` strength. `lockAfterHours` > 0 makes the type permanent
    // once held that long.
    types_ = {
        Type{"plain", {}, std::nullopt, 0.0, 0.0, false},

        // The first-morning payoff: a day of charcoal, tipped over by the morning feeding.
        Type{"soot",    {atLeast(P::Density, 0.70), atMost(P::Clarity, 0.25), atLeast(P::Acidity, 0.50)},
             std::nullopt, 0.0, 0.0, false},

        Type{"brook",   {atLeast(P::Clarity, 0.65), atMost(P::Heat, 0.40)},
             M::Water, 0.50, 0.0, false},

        // A brook slime kept in the dark long enough. More specific than brook, so it wins.
        Type{"lantern", {atLeast(P::Clarity, 0.80), atMost(P::Light, 0.30), atMost(P::Heat, 0.40)},
             M::Water, 0.60, 168.0, false},

        Type{"forge",   {atLeast(P::Heat, 0.75), atLeast(P::Density, 0.65)},
             M::Metal, 0.60, 168.0, false},

        Type{"frost",   {atMost(P::Heat, 0.20), atLeast(P::Density, 0.55)},
             std::nullopt, 0.0, 0.0, false},

        Type{"loam",    {between(P::Acidity, 0.55, 0.80), atLeast(P::Density, 0.55)},
             M::Wood, 0.50, 0.0, false},

        Type{"moss",    {atMost(P::Light, 0.40), atMost(P::Clarity, 0.45)},
             M::Wood, 0.50, 0.0, false},

        Type{"scrub",   {atLeast(P::Acidity, 0.70), atLeast(P::Memory, 0.65)},
             std::nullopt, 0.0, 0.0, false},

        Type{"puff",    {atMost(P::Density, 0.30), atLeast(P::Boldness, 0.65)},
             std::nullopt, 0.0, 0.0, false},

        Type{"shade",   {atMost(P::Boldness, 0.20), atMost(P::Light, 0.30)},
             std::nullopt, 0.0, 0.0, false},

        Type{"glutton", {atLeast(P::Appetite, 0.85)},
             std::nullopt, 0.0, 0.0, false},

        Type{"steady",  {atLeast(P::Memory, 0.85)},
             std::nullopt, 0.0, 0.0, false},

        // Rare: needs the cold cave and a stone diet for a long time.
        Type{"pearl",   {atLeast(P::Clarity, 0.90), atLeast(P::Density, 0.70), atMost(P::Heat, 0.25)},
             M::Stone, 0.70, 24.0, true},
    };
}

void Catalog::finish() {
    // Most specific first; stable so catalog order breaks ties.
    std::stable_sort(types_.begin(), types_.end(), [](const Type& a, const Type& b) {
        return a.specificity() > b.specificity();
    });
    plainIndex_ = static_cast<std::size_t>(type("plain") - types_.data());
}

const Food* Catalog::food(std::string_view id) const {
    for (const auto& f : foods_) if (f.id == id) return &f;
    return nullptr;
}
const Habitat* Catalog::habitat(std::string_view id) const {
    for (const auto& h : habitats_) if (h.id == id) return &h;
    return nullptr;
}
const Job* Catalog::job(std::string_view id) const {
    for (const auto& j : jobs_) if (j.id == id) return &j;
    return nullptr;
}
const Type* Catalog::type(std::string_view id) const {
    for (const auto& t : types_) if (t.id == id) return &t;
    return nullptr;
}

const Type* Catalog::resolve(const PropertyValues& v, Material affinity) const {
    for (const auto& t : types_) {
        if (t.matches(v, affinity)) return &t;
    }
    return &plain();
}

bool operator==(const Catalog& a, const Catalog& b) {
    return a.foods() == b.foods() && a.habitats() == b.habitats() && a.jobs() == b.jobs() &&
           a.types() == b.types();
}

} // namespace owlight
