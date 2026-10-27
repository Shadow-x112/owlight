#pragma once
// The catalog: foods, habitats, jobs, types. The numbers live in `data/*.json` and nowhere else.
// Catalog::load reads them from a directory; Catalog::v1() parses copies embedded at build time.

#include "owlight/Nudges.h"
#include "owlight/Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owlight {

// The four data files as text. Catalog::load reads them from disk; Catalog::v1 uses copies of
// the same files embedded at build time, so there is exactly one set of numbers.
struct CatalogTexts {
    std::string foods, habitats, jobs, types;
};

class Catalog {
public:
    // The shipped catalog, built from data/*.json embedded at build time. Needs no files on disk.
    static const Catalog& v1();

    // Parse a catalog from the texts of the four data files.
    static std::optional<Catalog> fromTexts(const CatalogTexts& texts, std::string* error = nullptr);

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
