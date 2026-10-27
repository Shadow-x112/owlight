#pragma once
// The catalog: foods, habitats, jobs, types. The game loads it from the data files in `data/`
// (Catalog::load); Catalog::v1() is the same catalog built in, used as the fallback and by tests.
// A test keeps the two identical, number for number.

#include "owlight/Nudges.h"
#include "owlight/Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owlight {

class Catalog {
public:
    static const Catalog& v1();

    // Load from a directory holding foods.json, habitats.json, jobs.json and types.json.
    // On failure returns nullopt and, if `error` is given, a message naming the file and line.
    // Slimes keep a reference to their catalog, so keep the loaded one alive while they live.
    static std::optional<Catalog> load(const std::string& dir, std::string* error = nullptr);

    const Food*    food(std::string_view id) const;
    const Habitat* habitat(std::string_view id) const;
    const Job*     job(std::string_view id) const;
    const Type*    type(std::string_view id) const;

    const std::vector<Food>&    foods() const { return foods_; }
    const std::vector<Habitat>& habitats() const { return habitats_; }
    const std::vector<Job>&     jobs() const { return jobs_; }
    const std::vector<Type>&    types() const { return types_; }   // sorted most specific first
    const Type&                 plain() const { return types_[plainIndex_]; }

    // Best matching type for these values, never null.
    const Type* resolve(const PropertyValues& v, Material affinity) const;

private:
    Catalog() = default;
    void fillBuiltIn();   // the v1 numbers, in Catalog.cpp
    void finish();   // sorts types most specific first; requires a "plain" type

    std::vector<Food> foods_;
    std::vector<Habitat> habitats_;
    std::vector<Job> jobs_;
    std::vector<Type> types_;
    std::size_t plainIndex_ = 0;   // an index, not a pointer, so a Catalog copies safely
};

// Exact comparison, number for number. Used to prove the data files match the built-in catalog.
bool operator==(const Catalog& a, const Catalog& b);
inline bool operator!=(const Catalog& a, const Catalog& b) { return !(a == b); }

} // namespace owlight
