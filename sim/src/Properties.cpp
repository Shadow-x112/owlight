#include "owlight/Properties.h"

namespace owlight {

std::string_view name(Property p) {
    switch (p) {
        case Property::Density:  return "density";
        case Property::Heat:     return "heat";
        case Property::Acidity:  return "acidity";
        case Property::Clarity:  return "clarity";
        case Property::Appetite: return "appetite";
        case Property::Boldness: return "boldness";
        case Property::Memory:   return "memory";
        case Property::Affinity: return "affinity";
        case Property::Light:    return "light";
        case Property::Bond:     return "bond";
        case Property::Count:    break;
    }
    return "?";
}

std::string_view name(Material m) {
    switch (m) {
        case Material::None:  return "none";
        case Material::Metal: return "metal";
        case Material::Wood:  return "wood";
        case Material::Cloth: return "cloth";
        case Material::Water: return "water";
        case Material::Stone: return "stone";
        case Material::Count: break;
    }
    return "?";
}

double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

} // namespace owlight
