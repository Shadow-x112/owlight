#pragma once
// The v1 catalog: foods, habitats, jobs, types. Numeric source of truth lives in Catalog.cpp.
// Later this will load from data files; the interface stays the same.

#include "owlight/Nudges.h"
#include "owlight/Types.h"

#include <string_view>
#include <vector>

namespace owlight {

class Catalog {
public:
    static const Catalog& v1();

    const Food*    food(std::string_view id) const;
    const Habitat* habitat(std::string_view id) const;
    const Job*     job(std::string_view id) const;
    const Type*    type(std::string_view id) const;

    const std::vector<Food>&    foods() const { return foods_; }
    const std::vector<Habitat>& habitats() const { return habitats_; }
    const std::vector<Job>&     jobs() const { return jobs_; }
    const std::vector<Type>&    types() const { return types_; }   // sorted most specific first
    const Type&                 plain() const { return *plain_; }

    // Best matching type for these values, never null.
    const Type* resolve(const PropertyValues& v, Material affinity) const;

private:
    Catalog();
    std::vector<Food> foods_;
    std::vector<Habitat> habitats_;
    std::vector<Job> jobs_;
    std::vector<Type> types_;
    const Type* plain_ = nullptr;
};

} // namespace owlight
