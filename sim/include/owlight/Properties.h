#pragma once
// Owlight simulation core — hidden slime properties.
// Standalone C++17. No engine dependency.

#include <array>
#include <cstddef>
#include <string_view>

namespace owlight {

// The ten hidden properties. See docs/design.md §5.1.
enum class Property : std::size_t {
    Density = 0,
    Heat,
    Acidity,
    Clarity,
    Appetite,
    Boldness,
    Memory,
    Affinity,   // strength of attachment to the current material (see Material)
    Light,
    Bond,
    Count
};

constexpr std::size_t kPropertyCount = static_cast<std::size_t>(Property::Count);

// Materials a slime can develop an affinity for. Property::Affinity holds the strength;
// the material itself is tracked separately on the slime.
enum class Material : std::size_t {
    None = 0,
    Metal,
    Wood,
    Cloth,
    Water,
    Stone,
    Count
};

constexpr std::size_t kMaterialCount = static_cast<std::size_t>(Material::Count);

std::string_view name(Property p);
std::string_view name(Material m);

// A full set of property values, each in [0, 1].
using PropertyValues = std::array<double, kPropertyCount>;

// Resting point every property drifts back toward when nothing pushes it.
constexpr double kRestingValue = 0.5;

inline double& at(PropertyValues& v, Property p) { return v[static_cast<std::size_t>(p)]; }
inline double  at(const PropertyValues& v, Property p) { return v[static_cast<std::size_t>(p)]; }

// Starting values for a new slime: the resting value plus a small deterministic variance, so
// individuals differ a little without breaking the rules. Not randomness: the same seed always
// gives the same slime, and seed 0 is exactly at rest.
PropertyValues starterValues(unsigned seed);

// Clamp a value into [0, 1].
double clamp01(double x);

} // namespace owlight
