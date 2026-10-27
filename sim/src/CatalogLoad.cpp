// Catalog::load — reads the catalog from the data files in data/. The format is documented in
// data/README.md. The loader is strict on purpose: these files are edited by hand while tuning,
// so a misspelt key or property is an error that names the file and line, never a silent zero.

#include "owlight/Catalog.h"

#include "Json.h"

#include <fstream>
#include <initializer_list>
#include <sstream>

namespace owlight {

namespace {

using json::Value;
using Kind = json::Value::Kind;

class Reader {
public:
    explicit Reader(std::string file) : file_(std::move(file)) {}

    const std::string& error() const { return error_; }

    bool fail(int line, const std::string& msg) {
        if (error_.empty()) error_ = file_ + ":" + std::to_string(line) + ": " + msg;
        return false;
    }
    bool fail(const Value& at, const std::string& msg) { return fail(at.line, msg); }

    bool parseText(const std::string& text, Value& out) {
        json::Error err;
        if (!json::parse(text, out, err)) return fail(err.line, err.message);
        return true;
    }

    bool expect(const Value& v, Kind k, const std::string& what) {
        if (v.kind == k) return true;
        return fail(v, what + " must be " + std::string(json::kindName(k)) + ", not " +
                           std::string(json::kindName(v.kind)));
    }

    // Every key in `obj` must be one of `allowed`.
    bool onlyKeys(const Value& obj, std::initializer_list<std::string_view> allowed,
                  const std::string& where) {
        for (const auto& m : obj.object) {
            bool ok = false;
            for (auto a : allowed) ok = ok || m.key == a;
            if (!ok) return fail(m.value, "unknown key \"" + m.key + "\" in " + where);
        }
        return true;
    }

    bool requiredString(const Value& obj, std::string_view key, std::string& out,
                        const std::string& where) {
        const Value* v = obj.find(key);
        if (!v) return fail(obj, where + " needs \"" + std::string(key) + "\"");
        if (!expect(*v, Kind::String, "\"" + std::string(key) + "\"")) return false;
        if (v->string.empty()) return fail(*v, "\"" + std::string(key) + "\" is empty");
        out = v->string;
        return true;
    }

    // Optional number; must lie in [lo, hi].
    bool number(const Value& obj, std::string_view key, double& out, double lo, double hi) {
        const Value* v = obj.find(key);
        if (!v) return true;
        if (!expect(*v, Kind::Number, "\"" + std::string(key) + "\"")) return false;
        if (v->number < lo || v->number > hi) {
            std::ostringstream msg;
            msg << "\"" << key << "\" must be between " << lo << " and " << hi;
            return fail(*v, msg.str());
        }
        out = v->number;
        return true;
    }

    bool boolean(const Value& obj, std::string_view key, bool& out) {
        const Value* v = obj.find(key);
        if (!v) return true;
        if (!expect(*v, Kind::Bool, "\"" + std::string(key) + "\"")) return false;
        out = v->boolean;
        return true;
    }

    bool property(const Value& at, const std::string& s, Property& out) {
        for (std::size_t i = 0; i < kPropertyCount; ++i) {
            if (name(static_cast<Property>(i)) == s) { out = static_cast<Property>(i); return true; }
        }
        return fail(at, "unknown property \"" + s + "\"");
    }

    bool material(const Value& obj, Material& out, bool allowNone) {
        const Value* v = obj.find("material");
        if (!v) return true;
        if (!expect(*v, Kind::String, "\"material\"")) return false;
        for (std::size_t i = allowNone ? 0 : 1; i < kMaterialCount; ++i) {
            if (name(static_cast<Material>(i)) == v->string) { out = static_cast<Material>(i); return true; }
        }
        return fail(*v, "unknown material \"" + v->string + "\"");
    }

    // { "density": 0.06, "clarity": -0.09 } — omitted properties are zero.
    bool deltas(const Value& obj, std::string_view key, Deltas& out) {
        out = noDeltas();
        const Value* v = obj.find(key);
        if (!v) return true;
        if (!expect(*v, Kind::Object, "\"" + std::string(key) + "\"")) return false;
        for (const auto& m : v->object) {
            Property p{};
            if (!property(m.value, m.key, p)) return false;
            if (!expect(m.value, Kind::Number, "\"" + m.key + "\"")) return false;
            at(out, p) = m.value.number;
        }
        return true;
    }

    // The top level of every data file is an array of entry objects with unique ids.
    template <class T, class Fn>
    bool entries(const Value& root, std::vector<T>& out, Fn readEntry) {
        if (!expect(root, Kind::Array, "the file")) return false;
        out.clear();
        for (const auto& e : root.array) {
            if (!expect(e, Kind::Object, "each entry")) return false;
            T item{};
            if (!requiredString(e, "id", item.id, "each entry")) return false;
            for (const auto& prev : out) {
                if (prev.id == item.id) return fail(*e.find("id"), "duplicate id \"" + item.id + "\"");
            }
            if (!readEntry(e, item)) return false;
            out.push_back(std::move(item));
        }
        return true;
    }

private:
    std::string file_;
    std::string error_;
};

constexpr double kBig = 1e9;

} // namespace

std::optional<Catalog> Catalog::load(const std::string& dir, std::string* error) {
    auto path = [&](const char* file) {
        if (dir.empty()) return std::string(file);
        const char last = dir.back();
        return (last == '/' || last == '\\') ? dir + file : dir + "/" + file;
    };
    CatalogTexts texts;
    for (auto [file, slot] : {std::pair{"foods.json", &texts.foods}, std::pair{"habitats.json", &texts.habitats},
                              std::pair{"jobs.json", &texts.jobs}, std::pair{"types.json", &texts.types}}) {
        std::ifstream in(path(file), std::ios::binary);
        if (!in) {
            if (error) *error = "cannot open " + path(file);
            return std::nullopt;
        }
        std::ostringstream text;
        text << in.rdbuf();
        *slot = text.str();
    }
    return fromTexts(texts, error);
}

std::optional<Catalog> Catalog::fromTexts(const CatalogTexts& texts, std::string* error) {
    Catalog c;
    auto failed = [&](const Reader& r) -> std::optional<Catalog> {
        if (error) *error = r.error();
        return std::nullopt;
    };

    {
        Reader r("foods.json");
        Value root;
        const bool ok = r.parseText(texts.foods, root) &&
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
        const bool ok = r.parseText(texts.habitats, root) &&
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
        const bool ok = r.parseText(texts.jobs, root) &&
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
        const bool ok = r.parseText(texts.types, root) &&
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
