// Slime::save / Slime::load — the slime's full state as JSON, exact to the last bit.
//
// Everything a resumed slime needs is here, including the bookkeeping (last sign bands, last
// stirring target, last habitat/job) that stops the journal from re-announcing things on load.
// Catalog entries are stored by id. Numbers are written with 17 significant digits so a
// save → load → save round trip is byte-identical.

#include "owlight/Catalog.h"
#include "owlight/Slime.h"

#include "Json.h"

#include <cstdio>
#include <locale>
#include <sstream>
#include <string>

namespace owlight {

namespace {

constexpr int kSaveVersion = 1;

using json::Value;
using Kind = json::Value::Kind;

// --- tiny JSON writer ---------------------------------------------------------------------

std::string num(double x) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", x);
    // Keep JSON valid: "inf"/"nan" cannot occur for our state, but never emit a bare "-0".
    std::string s = buf;
    if (s == "-0") s = "0";
    return s;
}

std::string str(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

// --- reader helpers ------------------------------------------------------------------------

struct Reader {
    std::string error;
    bool fail(const Value& at, const std::string& msg) {
        if (error.empty()) error = "save:" + std::to_string(at.line) + ": " + msg;
        return false;
    }
    bool need(const Value& obj, const char* key, Kind k, const Value*& out) {
        out = obj.find(key);
        if (!out) return fail(obj, std::string("missing \"") + key + "\"");
        if (out->kind != k) return fail(*out, std::string("\"") + key + "\" must be " + std::string(json::kindName(k)));
        return true;
    }
    bool number(const Value& obj, const char* key, double& out) {
        const Value* v; if (!need(obj, key, Kind::Number, v)) return false;
        out = v->number; return true;
    }
    bool string(const Value& obj, const char* key, std::string& out) {
        const Value* v; if (!need(obj, key, Kind::String, v)) return false;
        out = v->string; return true;
    }
    bool boolean(const Value& obj, const char* key, bool& out) {
        const Value* v; if (!need(obj, key, Kind::Bool, v)) return false;
        out = v->boolean; return true;
    }
    // string or null
    bool optString(const Value& obj, const char* key, std::string& out, bool& isNull) {
        const Value* v = obj.find(key);
        if (!v) return fail(obj, std::string("missing \"") + key + "\"");
        if (v->kind == Kind::Null) { isNull = true; return true; }
        if (v->kind != Kind::String) return fail(*v, std::string("\"") + key + "\" must be a string or null");
        isNull = false; out = v->string; return true;
    }
};

} // namespace

std::string Slime::save() const {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "{\n";
    o << "  \"version\": " << kSaveVersion << ",\n";
    o << "  \"name\": " << str(name_) << ",\n";
    o << "  \"named\": " << (named_ ? "true" : "false") << ",\n";
    o << "  \"clock\": " << num(clock_) << ",\n";
    o << "  \"last_fed_at\": " << num(lastFedAt_) << ",\n";

    o << "  \"values\": {";
    for (std::size_t i = 0; i < kPropertyCount; ++i)
        o << (i ? ", " : " ") << "\"" << owlight::name(static_cast<Property>(i)) << "\": " << num(values_[i]);
    o << " },\n";

    o << "  \"exposure\": {";
    bool first = true;
    for (std::size_t i = 1; i < kMaterialCount; ++i) {
        o << (first ? " " : ", ") << "\"" << owlight::name(static_cast<Material>(i)) << "\": " << num(exposure_[i]);
        first = false;
    }
    o << " },\n";
    o << "  \"affinity_material\": \"" << owlight::name(affinityMaterial_) << "\",\n";

    o << "  \"type\": " << str(type_->id) << ",\n";
    o << "  \"type_held_since\": " << num(typeHeldSince_) << ",\n";
    o << "  \"type_lost_since\": " << num(typeLostSince_) << ",\n";
    o << "  \"locked\": " << (locked_ ? "true" : "false") << ",\n";

    o << "  \"last_habitat\": " << (lastHabitat_ ? str(lastHabitat_->id) : "null") << ",\n";
    o << "  \"last_job\": " << (lastJob_ ? str(lastJob_->id) : "null") << ",\n";

    o << "  \"last_bands\": [";
    for (std::size_t i = 0; i < kPropertyCount; ++i) o << (i ? ", " : "") << lastBands_[i];
    o << "],\n";
    o << "  \"last_stirring\": " << str(lastStirring_) << ",\n";

    o << "  \"journal\": [";
    const auto& es = journal_.entries();
    for (std::size_t i = 0; i < es.size(); ++i) {
        o << (i ? ",\n    " : "\n    ") << "{ \"at\": " << num(es[i].atHours)
          << ", \"kind\": \"" << owlight::name(es[i].kind) << "\", \"text\": " << str(es[i].text) << " }";
    }
    o << (es.empty() ? "" : "\n  ") << "]\n";
    o << "}\n";
    return o.str();
}

std::optional<Slime> Slime::load(const std::string& text, const Catalog& catalog, Rates rates,
                                 std::string* error) {
    Reader r;
    auto failed = [&]() -> std::optional<Slime> {
        if (error) *error = r.error;
        return std::nullopt;
    };

    Value root;
    json::Error perr;
    if (!json::parse(text, root, perr)) {
        r.error = "save:" + std::to_string(perr.line) + ": " + perr.message;
        return failed();
    }
    if (root.kind != Kind::Object) { r.fail(root, "a save must be an object"); return failed(); }

    double version = 0;
    if (!r.number(root, "version", version)) return failed();
    if (version != kSaveVersion) { r.fail(*root.find("version"), "unsupported save version"); return failed(); }

    std::string name;
    if (!r.string(root, "name", name)) return failed();

    // Values first: they are what the constructor needs.
    PropertyValues values{};
    {
        const Value* v; if (!r.need(root, "values", Kind::Object, v)) return failed();
        std::array<bool, kPropertyCount> seen{};
        for (const auto& m : v->object) {
            bool ok = false;
            for (std::size_t i = 0; i < kPropertyCount; ++i) {
                if (owlight::name(static_cast<Property>(i)) == m.key) {
                    if (m.value.kind != Kind::Number) { r.fail(m.value, "\"" + m.key + "\" must be a number"); return failed(); }
                    if (m.value.number < 0.0 || m.value.number > 1.0) { r.fail(m.value, "\"" + m.key + "\" out of range"); return failed(); }
                    values[i] = m.value.number; seen[i] = true; ok = true;
                }
            }
            if (!ok) { r.fail(m.value, "unknown property \"" + m.key + "\""); return failed(); }
        }
        for (std::size_t i = 0; i < kPropertyCount; ++i)
            if (!seen[i]) { r.fail(*v, "missing value for \"" + std::string(owlight::name(static_cast<Property>(i))) + "\""); return failed(); }
    }

    Slime s(name, values, catalog, rates);
    s.journal_.clear();

    if (!r.boolean(root, "named", s.named_)) return failed();
    if (!r.number(root, "clock", s.clock_)) return failed();
    if (!r.number(root, "last_fed_at", s.lastFedAt_)) return failed();
    if (s.clock_ < 0.0) { r.fail(*root.find("clock"), "\"clock\" cannot be negative"); return failed(); }

    {
        const Value* v; if (!r.need(root, "exposure", Kind::Object, v)) return failed();
        s.exposure_.fill(0.0);
        for (const auto& m : v->object) {
            bool ok = false;
            for (std::size_t i = 1; i < kMaterialCount; ++i) {
                if (owlight::name(static_cast<Material>(i)) == m.key) {
                    if (m.value.kind != Kind::Number || m.value.number < 0.0) { r.fail(m.value, "\"" + m.key + "\" must be a number >= 0"); return failed(); }
                    s.exposure_[i] = m.value.number; ok = true;
                }
            }
            if (!ok) { r.fail(m.value, "unknown material \"" + m.key + "\""); return failed(); }
        }
    }
    {
        std::string mat;
        if (!r.string(root, "affinity_material", mat)) return failed();
        bool ok = false;
        for (std::size_t i = 0; i < kMaterialCount; ++i)
            if (owlight::name(static_cast<Material>(i)) == mat) { s.affinityMaterial_ = static_cast<Material>(i); ok = true; }
        if (!ok) { r.fail(*root.find("affinity_material"), "unknown material \"" + mat + "\""); return failed(); }
    }

    {
        std::string typeId;
        if (!r.string(root, "type", typeId)) return failed();
        if (!r.number(root, "type_held_since", s.typeHeldSince_)) return failed();
        if (!r.number(root, "type_lost_since", s.typeLostSince_)) return failed();
        if (!r.boolean(root, "locked", s.locked_)) return failed();
        if (const Type* t = catalog.type(typeId)) {
            s.type_ = t;
        } else {
            // The catalog changed under the save: re-resolve from the values and drop the lock.
            s.type_ = catalog.resolve(s.values_, s.affinityMaterial_);
            s.typeHeldSince_ = s.clock_;
            s.typeLostSince_ = -1.0;
            s.locked_ = false;
            s.journal_.add(s.clock_, EntryKind::Note, "it seems different than you remember");
        }
    }

    {
        std::string id; bool isNull = false;
        if (!r.optString(root, "last_habitat", id, isNull)) return failed();
        s.lastHabitat_ = isNull ? nullptr : catalog.habitat(id);   // unknown id → treated as none
        if (!r.optString(root, "last_job", id, isNull)) return failed();
        s.lastJob_ = isNull ? nullptr : catalog.job(id);
    }

    {
        const Value* v; if (!r.need(root, "last_bands", Kind::Array, v)) return failed();
        if (v->array.size() != kPropertyCount) { r.fail(*v, "\"last_bands\" must have one entry per property"); return failed(); }
        for (std::size_t i = 0; i < kPropertyCount; ++i) {
            const Value& b = v->array[i];
            if (b.kind != Kind::Number || b.number < -2 || b.number > 2 || b.number != static_cast<int>(b.number)) {
                r.fail(b, "band must be an integer in -2..2"); return failed();
            }
            s.lastBands_[i] = static_cast<int>(b.number);
        }
        if (!r.string(root, "last_stirring", s.lastStirring_)) return failed();
    }

    {
        const Value* v; if (!r.need(root, "journal", Kind::Array, v)) return failed();
        std::vector<Entry> entries;
        for (const auto& e : v->array) {
            if (e.kind != Kind::Object) { r.fail(e, "journal entries must be objects"); return failed(); }
            double at = 0; std::string kind, text;
            if (!r.number(e, "at", at) || !r.string(e, "kind", kind) || !r.string(e, "text", text)) return failed();
            auto k = entryKindFromName(kind);
            if (!k) { r.fail(*e.find("kind"), "unknown journal kind \"" + kind + "\""); return failed(); }
            entries.push_back(Entry{at, *k, text});
        }
        // Keep the catalog-change note (if any) after the restored entries.
        std::vector<Entry> note = s.journal_.entries();
        s.journal_.clear();
        for (auto& e : entries) s.journal_.add(e.atHours, e.kind, std::move(e.text));
        for (auto& e : note) s.journal_.add(e.atHours, e.kind, std::move(e.text));
    }

    if (error) error->clear();
    return s;
}

} // namespace owlight
