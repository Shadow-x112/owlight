#include "owlight/Types.h"

#include <algorithm>
#include <cmath>

namespace owlight {

bool Type::matches(const PropertyValues& v, Material affinityMaterial) const {
    if (material) {
        if (affinityMaterial != *material) return false;
        if (at(v, Property::Affinity) < minAffinity) return false;
    }
    for (const auto& c : conditions) {
        const double x = at(v, c.property);
        if (x < c.min || x > c.max) return false;
    }
    return true;
}

double Type::distance(const PropertyValues& v, Material affinityMaterial) const {
    double worst = 0.0;
    if (material) {
        if (affinityMaterial != *material) return 1.0;   // wrong material: not close at all
        worst = std::max(worst, minAffinity - at(v, Property::Affinity));
    }
    for (const auto& c : conditions) {
        const double x = at(v, c.property);
        if (x < c.min) worst = std::max(worst, c.min - x);
        else if (x > c.max) worst = std::max(worst, x - c.max);
    }
    return std::max(0.0, worst);
}

} // namespace owlight
