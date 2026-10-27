#pragma once
// Things the player does that push hidden properties: foods, habitats, jobs.
// Each is a named bundle of deltas. Deltas are per feeding (Food) or per hour (Habitat, Job).

#include "owlight/Properties.h"

#include <string>

namespace owlight {

// A delta per property. Zero means "no effect".
using Deltas = PropertyValues;

constexpr Deltas noDeltas() { return Deltas{}; }

struct Food {
    std::string id;
    Deltas perFeeding{};
    Material material = Material::None;   // some foods carry a material (ore → metal)
    double materialExposureHours = 0.0;   // how much "being around" that material one feeding counts as
};

struct Habitat {
    std::string id;
    Deltas perHour{};
    Material material = Material::None;   // dominant material of the place
    double daylight = 1.0;                // 0 = never sees daylight (cave), 1 = fully open
    bool loud = false;                    // near a road, a forge, a busy room
};

struct Job {
    std::string id;
    Deltas perHour{};
    Material material = Material::None;   // what it handles while working
};

} // namespace owlight
