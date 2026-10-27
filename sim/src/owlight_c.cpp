// The C interface. Thin: every function forwards to the C++ core and keeps string storage in
// the handle so the returned pointers stay valid until the next change to that slime.

#define OWLIGHT_BUILD
#include "owlight/owlight_c.h"

#include "owlight/Catalog.h"
#include "owlight/Scenario.h"
#include "owlight/Slime.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace owlight;

// The opaque types. owl_catalog wraps an owned Catalog, or points at the built-in one.
struct owl_catalog {
    std::optional<Catalog> owned;
    const Catalog* ref = nullptr;
    const Catalog& get() const { return owned ? *owned : *ref; }
};

struct owl_slime {
    Slime slime;
    const Catalog& catalog;
    // String storage for returned pointers; refreshed lazily.
    std::vector<std::string> signTexts;
    std::vector<int> signProps;
    std::string saveText;
    explicit owl_slime(Slime s, const Catalog& c) : slime(std::move(s)), catalog(c) {}
};

namespace {

void copyError(const std::string& msg, char* err, size_t cap) {
    if (!err || cap == 0) return;
    const size_t n = msg.size() < cap - 1 ? msg.size() : cap - 1;
    std::memcpy(err, msg.data(), n);
    err[n] = '\0';
}

const owl_catalog& builtinHandle() {
    static owl_catalog h = [] { owl_catalog c; c.ref = &Catalog::v1(); return c; }();
    return h;
}

} // namespace

extern "C" {

const char* owl_version(void) { return "owlight-sim 0.1"; }

/* --- Catalog ----------------------------------------------------------------------------- */

const owl_catalog* owl_catalog_builtin(void) { return &builtinHandle(); }

owl_catalog* owl_catalog_load(const char* dir, char* err, size_t err_cap) {
    if (!dir) { copyError("dir is null", err, err_cap); return nullptr; }
    std::string msg;
    auto c = Catalog::load(dir, &msg);
    if (!c) { copyError(msg, err, err_cap); return nullptr; }
    auto* h = new owl_catalog;
    h->owned = std::move(*c);
    return h;
}

void owl_catalog_free(owl_catalog* catalog) {
    if (catalog && catalog != &builtinHandle()) delete catalog;
}

int owl_catalog_count(const owl_catalog* catalog, enum owl_kind kind) {
    if (!catalog) return 0;
    const Catalog& c = catalog->get();
    switch (kind) {
        case OWL_FOOD:    return static_cast<int>(c.foods().size());
        case OWL_HABITAT: return static_cast<int>(c.habitats().size());
        case OWL_JOB:     return static_cast<int>(c.jobs().size());
        case OWL_TYPE:    return static_cast<int>(c.types().size());
    }
    return 0;
}

const char* owl_catalog_id(const owl_catalog* catalog, enum owl_kind kind, int index) {
    if (!catalog || index < 0) return nullptr;
    const Catalog& c = catalog->get();
    const auto i = static_cast<size_t>(index);
    switch (kind) {
        case OWL_FOOD:    return i < c.foods().size()    ? c.foods()[i].id.c_str()    : nullptr;
        case OWL_HABITAT: return i < c.habitats().size() ? c.habitats()[i].id.c_str() : nullptr;
        case OWL_JOB:     return i < c.jobs().size()     ? c.jobs()[i].id.c_str()     : nullptr;
        case OWL_TYPE:    return i < c.types().size()    ? c.types()[i].id.c_str()    : nullptr;
    }
    return nullptr;
}

int owl_catalog_has(const owl_catalog* catalog, enum owl_kind kind, const char* id) {
    if (!catalog || !id) return 0;
    const Catalog& c = catalog->get();
    switch (kind) {
        case OWL_FOOD:    return c.food(id) != nullptr;
        case OWL_HABITAT: return c.habitat(id) != nullptr;
        case OWL_JOB:     return c.job(id) != nullptr;
        case OWL_TYPE:    return c.type(id) != nullptr;
    }
    return 0;
}

int owl_catalog_type_is_rare(const owl_catalog* catalog, const char* type_id) {
    if (!catalog || !type_id) return -1;
    const Type* t = catalog->get().type(type_id);
    return t ? (t->rare ? 1 : 0) : -1;
}

/* --- Slime lifetime ------------------------------------------------------------------------- */

owl_slime* owl_slime_new(const owl_catalog* catalog, const char* name, unsigned seed) {
    if (!catalog) return nullptr;
    const Catalog& c = catalog->get();
    return new owl_slime(Slime(name ? name : "slime", starterValues(seed), c), c);
}

owl_slime* owl_slime_load(const owl_catalog* catalog, const char* json, char* err, size_t err_cap) {
    if (!catalog || !json) { copyError("catalog or json is null", err, err_cap); return nullptr; }
    const Catalog& c = catalog->get();
    std::string msg;
    auto s = Slime::load(json, c, {}, &msg);
    if (!s) { copyError(msg, err, err_cap); return nullptr; }
    return new owl_slime(std::move(*s), c);
}

void owl_slime_free(owl_slime* slime) { delete slime; }

size_t owl_slime_save(const owl_slime* slime, char* buf, size_t buf_cap) {
    if (!slime) { if (buf && buf_cap) buf[0] = '\0'; return 0; }
    const std::string text = slime->slime.save();
    if (buf && buf_cap) {
        if (text.size() + 1 <= buf_cap) {
            std::memcpy(buf, text.data(), text.size());
            buf[text.size()] = '\0';
        } else {
            buf[0] = '\0';
        }
    }
    return text.size();
}

/* --- Actions ------------------------------------------------------------------------------- */

int owl_slime_feed(owl_slime* slime, const char* food_id) {
    if (!slime || !food_id) return -1;
    const Food* f = slime->catalog.food(food_id);
    if (!f) return -1;
    slime->slime.feed(*f, slime->slime.ageHours());
    return 0;
}

void owl_slime_pick_up(owl_slime* slime) {
    if (slime) slime->slime.pickUp(slime->slime.ageHours());
}

void owl_slime_give_name(owl_slime* slime, const char* name) {
    if (slime && name) slime->slime.giveName(name, slime->slime.ageHours());
}

int owl_slime_tick(owl_slime* slime, double hours, const char* habitat_id, const char* job_id,
                   int near_player, int is_daytime) {
    if (!slime) return -1;
    if (!(hours > 0.0) || std::isnan(hours)) return -3;
    Context ctx;
    ctx.habitat = habitat_id ? slime->catalog.habitat(habitat_id) : nullptr;
    if (habitat_id && !ctx.habitat) return -1;
    if (job_id) {
        ctx.job = slime->catalog.job(job_id);
        if (!ctx.job) return -2;
    }
    ctx.nearPlayer = near_player != 0;
    ctx.isDaytime = is_daytime != 0;
    slime->slime.tick(hours, ctx);
    return 0;
}

/* --- What the player can see ------------------------------------------------------------- */

const char* owl_slime_name(const owl_slime* slime) { return slime ? slime->slime.name().c_str() : ""; }
const char* owl_slime_type(const owl_slime* slime) { return slime ? slime->slime.type()->id.c_str() : ""; }
int owl_slime_locked(const owl_slime* slime) { return slime && slime->slime.locked() ? 1 : 0; }
double owl_slime_age_hours(const owl_slime* slime) { return slime ? slime->slime.ageHours() : 0.0; }

int owl_slime_sign_count(owl_slime* slime) {
    if (!slime) return 0;
    slime->signTexts.clear();
    slime->signProps.clear();
    for (const auto& s : slime->slime.signs()) {
        if (s.text.empty()) continue;
        slime->signTexts.push_back(s.text);
        slime->signProps.push_back(static_cast<int>(s.property));
    }
    return static_cast<int>(slime->signTexts.size());
}

const char* owl_slime_sign(const owl_slime* slime, int index) {
    if (!slime || index < 0 || static_cast<size_t>(index) >= slime->signTexts.size()) return nullptr;
    return slime->signTexts[static_cast<size_t>(index)].c_str();
}

int owl_slime_sign_property(const owl_slime* slime, int index) {
    if (!slime || index < 0 || static_cast<size_t>(index) >= slime->signProps.size()) return -1;
    return slime->signProps[static_cast<size_t>(index)];
}

int owl_slime_journal_count(const owl_slime* slime) {
    return slime ? static_cast<int>(slime->slime.journal().entries().size()) : 0;
}

int owl_slime_journal_entry(const owl_slime* slime, int index, double* at_hours, const char** kind,
                            const char** text) {
    if (!slime || index < 0) return -1;
    const auto& es = slime->slime.journal().entries();
    if (static_cast<size_t>(index) >= es.size()) return -1;
    const Entry& e = es[static_cast<size_t>(index)];
    if (at_hours) *at_hours = e.atHours;
    if (kind) *kind = name(e.kind).data();   // names are string literals
    if (text) *text = e.text.c_str();
    return 0;
}

/* --- Hidden ------------------------------------------------------------------------------- */

double owl_slime_value(const owl_slime* slime, enum owl_property property) {
    if (!slime || property < 0 || property >= OWL_PROPERTY_COUNT) return std::numeric_limits<double>::quiet_NaN();
    return slime->slime.values()[static_cast<size_t>(property)];
}

const char* owl_slime_affinity_material(const owl_slime* slime) {
    return slime ? name(slime->slime.affinityMaterial()).data() : "none";
}

} // extern "C"
