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

// Exact comparisons, number for number (see Catalog's operator==).
inline bool operator==(const Food& a, const Food& b) {
    return a.id == b.id && a.perFeeding == b.perFeeding && a.material == b.material &&
           a.materialExposureHours == b.materialExposureHours;
}
inline bool operator==(const Habitat& a, const Habitat& b) {
    return a.id == b.id && a.perHour == b.perHour && a.material == b.material &&
           a.daylight == b.daylight && a.loud == b.loud;
}
inline bool operator==(const Job& a, const Job& b) {
    return a.id == b.id && a.perHour == b.perHour && a.material == b.material;
}
inline bool operator!=(const Food& a, const Food& b) { return !(a == b); }
inline bool operator!=(const Habitat& a, const Habitat& b) { return !(a == b); }
inline bool operator!=(const Job& a, const Job& b) { return !(a == b); }

} // namespace owlight
