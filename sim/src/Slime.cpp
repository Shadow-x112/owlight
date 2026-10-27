#include "owlight/Slime.h"

#include "owlight/Catalog.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace owlight {

namespace {
constexpr double kMaxStepHours = 1.0;   // integrate in steps no larger than this
}

Slime::Slime(std::string name, PropertyValues initial, const Catalog& catalog, Rates rates)
    : name_(std::move(name)), catalog_(catalog), rates_(rates) {
    for (std::size_t i = 0; i < kPropertyCount; ++i) values_[i] = clamp01(initial[i]);
    type_ = catalog_.resolve(values_, affinityMaterial_);
    typeHeldSince_ = 0.0;
    for (std::size_t i = 0; i < kPropertyCount; ++i) lastBands_[i] = 0;
    for (const auto& s : signsFor(values_)) lastBands_[static_cast<std::size_t>(s.property)] = s.band;
}

// Actions --------------------------------------------------------------------------------

void Slime::feed(const Food& food, double atHours) {
    applyDeltas(food.perFeeding, 1.0);
    at(values_, Property::Appetite) = clamp01(at(values_, Property::Appetite) + rates_.feedAppetite);
    exposeToMaterial(food.material, food.materialExposureHours);
    updateAffinity();
    lastFedAt_ = atHours;
    journal_.add(atHours, EntryKind::Fed, "ate " + food.id);
    checkSigns();
    resolveType();
    checkStirring();
}

void Slime::pickUp(double atHours) {
    at(values_, Property::Bond) = clamp01(at(values_, Property::Bond) + rates_.bondPickUp);
    (void)atHours;
}

void Slime::giveName(std::string newName, double atHours) {
    name_ = std::move(newName);
    if (!named_) {
        named_ = true;
        at(values_, Property::Bond) = clamp01(at(values_, Property::Bond) + rates_.bondName);
        journal_.add(atHours, EntryKind::Note, "named it " + name_);
    }
}

// Time -------------------------------------------------------------------------------------

void Slime::tick(double hours, const Context& ctx) {
    if (hours <= 0.0) return;

    if (ctx.habitat != lastHabitat_) {
        journal_.add(clock_, EntryKind::Moved, ctx.habitat ? "moved to " + ctx.habitat->id : "left its habitat");
        lastHabitat_ = ctx.habitat;
    }
    if (ctx.job != lastJob_) {
        if (ctx.job) {
            journal_.add(clock_, EntryKind::Assigned, "set to " + ctx.job->id);
            if (lastJob_) {
                at(values_, Property::Memory) = clamp01(at(values_, Property::Memory) - rates_.memorySwitchPenalty);
            }
        } else {
            journal_.add(clock_, EntryKind::Assigned, "given nothing to do");
        }
        lastJob_ = ctx.job;
    }

    double remaining = hours;
    while (remaining > 1e-9) {
        const double h = std::min(remaining, kMaxStepHours);
        step(h, ctx);
        remaining -= h;
    }
}

void Slime::step(double h, const Context& ctx) {
    // 1. Drift toward the resting point, proportional to distance.
    for (std::size_t i = 0; i < kPropertyCount; ++i) {
        if (i == static_cast<std::size_t>(Property::Affinity)) continue;   // affinity has its own rule
        values_[i] -= (values_[i] - kRestingValue) * rates_.driftPerHour * h;
    }

    // 2. Habitat and job pushes.
    decayExposure(h);
    if (ctx.habitat) {
        applyDeltas(ctx.habitat->perHour, h);
        exposeToMaterial(ctx.habitat->material, h * rates_.habitatExposureWeight);
    }
    if (ctx.job) {
        applyDeltas(ctx.job->perHour, h);
        at(values_, Property::Memory) += rates_.memoryWorkPerHour * h;
        exposeToMaterial(ctx.job->material, h);
    }
    updateAffinity();

    // 3. Light: daylight exposure raises, darkness lowers. Habitat scales how much day reaches it.
    {
        const double daylight = ctx.habitat ? ctx.habitat->daylight : 1.0;
        const double exposure = ctx.isDaytime ? daylight : 0.0;     // 0..1
        at(values_, Property::Light) += (exposure - 0.5) * rates_.lightPerHour * h;
    }

    // 4. Appetite: falls once it has gone hungry.
    if (clock_ - lastFedAt_ > rates_.starveAfterHours) {
        at(values_, Property::Appetite) -= rates_.starvePerHour * h;
    }

    // 5. Boldness and bond: company matters.
    if (ctx.nearPlayer) {
        at(values_, Property::Boldness) += rates_.boldNearPerHour * h;
        at(values_, Property::Bond)     += rates_.bondNearPerHour * h;
    } else {
        at(values_, Property::Boldness) -= rates_.boldAlonePerHour * h;
        at(values_, Property::Bond)     -= rates_.bondAlonePerHour * h;
    }
    if (ctx.habitat && ctx.habitat->loud) at(values_, Property::Boldness) += rates_.boldLoudPerHour * h;

    for (auto& v : values_) v = clamp01(v);
    clock_ += h;

    checkSigns();
    resolveType();
    checkStirring();
}

void Slime::applyDeltas(const Deltas& d, double scale) {
    for (std::size_t i = 0; i < kPropertyCount; ++i) {
        if (i == static_cast<std::size_t>(Property::Affinity)) continue;
        values_[i] = clamp01(values_[i] + d[i] * scale);
    }
}

// Affinity: the material it has been around most, with recent time counting more.
// Exposure is a weighted hour count per material that fades over ~a week. The affinity
// material is the largest; its strength is that material's share, discounted while the
// slime has not been around much of anything yet.
void Slime::exposeToMaterial(Material m, double hours) {
    if (m == Material::None || hours <= 0.0) return;
    exposure_[static_cast<std::size_t>(m)] += hours;
}

void Slime::decayExposure(double hours) {
    const double keep = std::exp(-hours / rates_.affinityMemoryHours);
    for (auto& e : exposure_) e *= keep;
}

void Slime::updateAffinity() {
    double total = 0.0, best = 0.0;
    Material bestMaterial = Material::None;
    for (std::size_t i = 1; i < kMaterialCount; ++i) {   // skip None
        total += exposure_[i];
        if (exposure_[i] > best) { best = exposure_[i]; bestMaterial = static_cast<Material>(i); }
    }
    affinityMaterial_ = bestMaterial;
    at(values_, Property::Affinity) = total > 0.0 ? clamp01(best / (total + rates_.affinityBaselineHours)) : 0.0;
}

// Types ------------------------------------------------------------------------------------

void Slime::resolveType() {
    if (locked_) return;

    const Type* now = catalog_.resolve(values_, affinityMaterial_);

    if (now == type_) {
        typeLostSince_ = -1.0;
        if (type_->lockAfterHours > 0.0 && clock_ - typeHeldSince_ >= type_->lockAfterHours) {
            locked_ = true;
            journal_.add(clock_, EntryKind::Locked, "it will stay a " + type_->id + " now");
        }
        return;
    }

    // Still matches its own thresholds (a more specific type appeared)? Switch up at once.
    if (type_ != &catalog_.plain() && !type_->matches(values_, affinityMaterial_)) {
        // Lost its type: start / continue the grace period.
        if (typeLostSince_ < 0.0) typeLostSince_ = clock_;
        if (clock_ - typeLostSince_ < rates_.revertGraceHours) return;
        journal_.add(clock_, EntryKind::Reverted,
                     "no longer a " + type_->id + (now == &catalog_.plain() ? "" : "; now a " + now->id));
    } else {
        journal_.add(clock_, EntryKind::Became, "became a " + now->id);
    }
    type_ = now;
    typeHeldSince_ = clock_;
    typeLostSince_ = -1.0;
}

// Journal ----------------------------------------------------------------------------------

void Slime::checkSigns() {
    for (const auto& s : signsFor(values_, lastBands_, rates_.signHysteresis)) {
        auto& last = lastBands_[static_cast<std::size_t>(s.property)];
        if (s.band != last) {
            last = s.band;
            if (!s.text.empty()) journal_.add(clock_, EntryKind::SignChanged, s.text);
        }
    }
}

void Slime::checkStirring() {
    // Report the nearest type we don't have, if it is close and not what we already are.
    const Type* nearest = nullptr;
    double best = rates_.stirringDistance;
    for (const auto& t : catalog_.types()) {
        if (&t == type_ || &t == &catalog_.plain()) continue;
        if (t.specificity() <= type_->specificity() && type_ != &catalog_.plain()) continue;
        const double d = t.distance(values_, affinityMaterial_);
        if (d > 0.0 && d < best) { best = d; nearest = &t; }
    }
    const std::string key = nearest ? nearest->id : std::string{};
    if (key != lastStirring_) {
        lastStirring_ = key;
        if (nearest) journal_.add(clock_, EntryKind::Stirring, "something is changing in it");
    }
}

} // namespace owlight
