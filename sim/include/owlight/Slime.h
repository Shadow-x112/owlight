#pragma once
// A single slime: hidden properties, what is pushing them right now, and the journal of what
// the player could have noticed. All change happens in tick() so time is the only integrator.

#include "owlight/Journal.h"
#include "owlight/Nudges.h"
#include "owlight/Properties.h"
#include "owlight/Types.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace owlight {

class Catalog;

// What surrounds the slime during a tick. Pointers are non-owning and may be null.
struct Context {
    const Habitat* habitat = nullptr;
    const Job* job = nullptr;
    bool nearPlayer = false;      // within a few meters, most of the time
    bool isDaytime = true;        // caller decides from the real clock; habitat.daylight scales it
};

// Tunable rates. Hours are real hours. Changing these changes the whole feel; see tests.
struct Rates {
    double driftPerHour        = 0.010;   // proportional pull toward kRestingValue
    double starveAfterHours    = 12.0;    // appetite starts falling after this long unfed
    double starvePerHour       = 0.004;
    double feedAppetite        = 0.03;    // per feeding
    double boldNearPerHour     = 0.004;
    double boldAlonePerHour    = 0.002;
    double boldLoudPerHour     = 0.002;
    double memoryWorkPerHour   = 0.006;
    double memorySwitchPenalty = 0.05;
    double lightPerHour        = 0.008;   // +/- depending on day/night exposure
    double bondNearPerHour     = 0.002;
    double bondAlonePerHour    = 0.001;
    double bondPickUp          = 0.03;
    double bondName            = 0.10;
    double affinityMemoryHours = 168.0;   // exposure fades with this time constant (~a week)
    double affinityBaselineHours = 24.0;  // how much exposure it takes before affinity means much
    double habitatExposureWeight = 0.5;   // being somewhere counts half as much as handling it
    double revertGraceHours    = 12.0;    // how long a lost type lingers before reverting
    double stirringDistance    = 0.12;    // "something is stirring" when within this of a type
    double signHysteresis      = 0.03;    // a sign must cross its line by this much to change
};

class Slime {
public:
    Slime(std::string name, PropertyValues initial, const Catalog& catalog, Rates rates = {});

    // Actions ---------------------------------------------------------------
    void feed(const Food& food, double atHours);
    void pickUp(double atHours);
    void giveName(std::string newName, double atHours);

    // Advance the simulation. Safe for any positive `hours`; internally sub-stepped.
    void tick(double hours, const Context& ctx);

    // Persistence ------------------------------------------------------------
    // A save is a JSON text holding everything needed to resume exactly: values, exposure, type,
    // lock, journal, and the bookkeeping that keeps signs and stirring from re-firing. Foods,
    // habitats, jobs and types are referenced by id so a save survives a catalog change; a type
    // id missing from the catalog is re-resolved from the values on load.
    std::string save() const;
    static std::optional<Slime> load(const std::string& text, const Catalog& catalog,
                                     Rates rates = {}, std::string* error = nullptr);

    // Queries ---------------------------------------------------------------
    const std::string& name() const { return name_; }
    const PropertyValues& values() const { return values_; }     // hidden; tests/tools only
    Material affinityMaterial() const { return affinityMaterial_; }
    const Type* type() const { return type_; }                   // never null; Plain by default
    bool locked() const { return locked_; }
    double ageHours() const { return clock_; }
    const Journal& journal() const { return journal_; }
    Journal& journal() { return journal_; }
    std::vector<Sign> signs() const { return signsFor(values_, lastBands_, rates_.signHysteresis); }

private:
    void step(double h, const Context& ctx);
    void applyDeltas(const Deltas& d, double scale);
    void exposeToMaterial(Material m, double hours);
    void decayExposure(double hours);
    void updateAffinity();
    void resolveType();
    void checkSigns();
    void checkStirring();

    std::string name_;
    PropertyValues values_{};
    Material affinityMaterial_ = Material::None;
    std::array<double, kMaterialCount> exposure_{};   // weighted hours around each material
    const Catalog& catalog_;
    Rates rates_;

    double clock_ = 0.0;              // hours since creation
    double lastFedAt_ = 0.0;
    const Job* lastJob_ = nullptr;
    const Habitat* lastHabitat_ = nullptr;
    bool named_ = false;

    const Type* type_ = nullptr;
    double typeHeldSince_ = 0.0;
    double typeLostSince_ = -1.0;     // <0 = not currently lost
    bool locked_ = false;

    std::array<int, kPropertyCount> lastBands_{};
    std::string lastStirring_;
    Journal journal_;
};

} // namespace owlight
