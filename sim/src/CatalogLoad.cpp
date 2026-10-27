// Catalog::load — reads the catalog from the data files in data/. The format is documented in
// data/README.md. The loader is strict on purpose: these files are edited by hand while tuning,
// so a misspelt key or property is an error that names the file and line, never a silent zero.

#include "owlight/Catalog.h"

#include "DataReader.h"

namespace owlight {

using data::Kind;
using data::kBig;
using data::Reader;
using data::Value;

std::optional<Catalog> Catalog::load(const std::string& dir, std::string* error) {
    Catalog c;
    auto path = [&](const char* file) {
        if (dir.empty()) return std::string(file);
        const char last = dir.back();
        return (last == '/' || last == '\\') ? dir + file : dir + "/" + file;
    };
    auto failed = [&](const Reader& r) -> std::optional<Catalog> {
        if (error) *error = r.error();
        return std::nullopt;
    };

    {
        Reader r("foods.json");
        Value root;
        const bool ok = r.parseFile(path("foods.json"), root) &&
            r.entries(root, c.foods_, [&](const Value& e, Food& f) {
                return r.onlyKeys(e, {"id", "note", "per_feeding", "material", "exposure_hours"},
                                  "food \"" + f.id + "\"") &&
                       r.deltas(e, "per_feeding", f.perFeeding) &&
                       r.material(e, f.material, true) &&
                       r.number(e, "exposure_hours", f.materialExposureHours, 0.0, kBig);
            });
        if (!ok) return failed(r);
    }
    {
        Reader r("habitats.json");
        Value root;
        const bool ok = r.parseFile(path("habitats.json"), root) &&
            r.entries(root, c.habitats_, [&](const Value& e, Habitat& h) {
                return r.onlyKeys(e, {"id", "note", "per_hour", "material", "daylight", "loud"},
                                  "habitat \"" + h.id + "\"") &&
                       r.deltas(e, "per_hour", h.perHour) &&
                       r.material(e, h.material, true) &&
                       r.number(e, "daylight", h.daylight, 0.0, 1.0) &&
                       r.boolean(e, "loud", h.loud);
            });
        if (!ok) return failed(r);
    }
    {
        Reader r("jobs.json");
        Value root;
        const bool ok = r.parseFile(path("jobs.json"), root) &&
            r.entries(root, c.jobs_, [&](const Value& e, Job& j) {
                return r.onlyKeys(e, {"id", "note", "per_hour", "material"}, "job \"" + j.id + "\"") &&
                       r.deltas(e, "per_hour", j.perHour) &&
                       r.material(e, j.material, true);
            });
        if (!ok) return failed(r);
    }
    {
        Reader r("types.json");
        Value root;
        const bool ok = r.parseFile(path("types.json"), root) &&
            r.entries(root, c.types_, [&](const Value& e, Type& t) {
                const std::string where = "type \"" + t.id + "\"";
                if (!r.onlyKeys(e, {"id", "note", "conditions", "material", "min_affinity",
                                    "lock_after_hours", "rare"}, where)) return false;
                if (const Value* conds = e.find("conditions")) {
                    if (!r.expect(*conds, Kind::Object, "\"conditions\"")) return false;
                    // { "density": { "min": 0.70 }, "acidity": { "min": 0.55, "max": 0.80 } }
                    for (const auto& m : conds->object) {
                        Condition cond{};
                        if (!r.property(m.value, m.key, cond.property)) return false;
                        if (!r.expect(m.value, Kind::Object, "condition \"" + m.key + "\"")) return false;
                        if (!r.onlyKeys(m.value, {"min", "max"}, "condition \"" + m.key + "\"")) return false;
                        if (m.value.object.empty()) return r.fail(m.value, "condition \"" + m.key + "\" needs \"min\" or \"max\"");
                        if (!r.number(m.value, "min", cond.min, 0.0, 1.0)) return false;
                        if (!r.number(m.value, "max", cond.max, 0.0, 1.0)) return false;
                        if (cond.min > cond.max) return r.fail(m.value, "condition \"" + m.key + "\" has min above max");
                        t.conditions.push_back(cond);
                    }
                }
                if (e.find("material")) {
                    Material mat = Material::None;
                    if (!r.material(e, mat, false)) return false;
                    t.material = mat;
                } else if (e.find("min_affinity")) {
                    return r.fail(*e.find("min_affinity"), "\"min_affinity\" needs a \"material\"");
                }
                return r.number(e, "min_affinity", t.minAffinity, 0.0, 1.0) &&
                       r.number(e, "lock_after_hours", t.lockAfterHours, 0.0, kBig) &&
                       r.boolean(e, "rare", t.rare);
            });
        if (!ok) return failed(r);
        if (!c.type("plain")) {
            r.fail(root, "there must be a type with id \"plain\"");
            return failed(r);
        }
    }

    c.finish();
    if (error) error->clear();
    return c;
}

} // namespace owlight
