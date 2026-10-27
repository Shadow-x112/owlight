// Catalog: lookup, type resolution, and the shipped catalog.
//
// There are no numbers in this file. The foods, habitats, jobs and type thresholds live in
// data/*.json and nowhere else; Catalog::v1() parses copies of those files that CMake embeds
// at build time (EmbeddedData.cpp), so the shipped catalog needs no files on disk and can never
// drift from data/. Design intent for every entry is in docs/design.md §5–6; the scale of the
// numbers is explained in data/README.md.

#include "owlight/Catalog.h"

#include "EmbeddedData.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace owlight {

const Catalog& Catalog::v1() {
    static const Catalog instance = [] {
        CatalogTexts texts{embedded::kFoods, embedded::kHabitats, embedded::kJobs, embedded::kTypes};
        std::string error;
        auto c = fromTexts(texts, &error);
        if (!c) {
            // The embedded data is validated by the test suite; reaching here means a broken build.
            std::fprintf(stderr, "owlight: embedded catalog is invalid: %s\n", error.c_str());
            std::abort();
        }
        return *c;
    }();
    return instance;
}

void Catalog::finish() {
    // Most specific first; stable so file order breaks ties.
    std::stable_sort(types_.begin(), types_.end(), [](const Type& a, const Type& b) {
        return a.specificity() > b.specificity();
    });
    plainIndex_ = static_cast<std::size_t>(type("plain") - types_.data());
}

const Food* Catalog::food(std::string_view id) const {
    for (const auto& f : foods_) if (f.id == id) return &f;
    return nullptr;
}
const Habitat* Catalog::habitat(std::string_view id) const {
    for (const auto& h : habitats_) if (h.id == id) return &h;
    return nullptr;
}
const Job* Catalog::job(std::string_view id) const {
    for (const auto& j : jobs_) if (j.id == id) return &j;
    return nullptr;
}
const Type* Catalog::type(std::string_view id) const {
    for (const auto& t : types_) if (t.id == id) return &t;
    return nullptr;
}

const Type* Catalog::resolve(const PropertyValues& v, Material affinity) const {
    for (const auto& t : types_) {
        if (t.matches(v, affinity)) return &t;
    }
    return &plain();
}

bool operator==(const Catalog& a, const Catalog& b) {
    return a.foods() == b.foods() && a.habitats() == b.habitats() && a.jobs() == b.jobs() &&
           a.types() == b.types();
}

} // namespace owlight
