#pragma once
// Slime types: sets of thresholds on the hidden properties. See docs/design.md §5.2, §6.

#include "owlight/Properties.h"

#include <optional>
#include <string>
#include <vector>

namespace owlight {

struct Condition {
    Property property;
    double min = 0.0;   // inclusive
    double max = 1.0;   // inclusive
};

struct Type {
    std::string id;                       // internal id; the player names types themselves
    std::vector<Condition> conditions;    // all must hold
    std::optional<Material> material;     // if set, the slime's affinity material must match
    double minAffinity = 0.0;             // required Affinity strength when material is set
    double lockAfterHours = 0.0;          // 0 = never locks; otherwise locks in once held this long
    bool rare = false;

    // More conditions = more specific. Ties broken by catalog order.
    std::size_t specificity() const { return conditions.size() + (material ? 1 : 0); }

    bool matches(const PropertyValues& v, Material affinityMaterial) const;

    // How close the values are to satisfying this type: 0 = matches, larger = further away.
    // Used by the journal to say "something is stirring".
    double distance(const PropertyValues& v, Material affinityMaterial) const;
};

} // namespace owlight
