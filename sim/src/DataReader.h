#pragma once
// Strict reading of hand-edited JSON data (catalog, scenarios). Internal to the sim. Every
// helper reports the first problem as "file:line: message" and returns false, so readers chain
// them with && and stop at the first error.

#include "owlight/Nudges.h"
#include "owlight/Properties.h"

#include "Json.h"

#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace owlight::data {

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

    bool parseFile(const std::string& path, Value& out) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            error_ = "cannot open " + path;
            return false;
        }
        std::ostringstream text;
        text << in.rdbuf();
        return parseText(text.str(), out);
    }

    bool parseText(std::string_view text, Value& out) {
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

    // Optional whole number in [lo, hi].
    bool integer(const Value& obj, std::string_view key, long long& out, long long lo, long long hi) {
        const Value* v = obj.find(key);
        if (!v) return true;
        if (!expect(*v, Kind::Number, "\"" + std::string(key) + "\"")) return false;
        const double x = v->number;
        if (x != static_cast<double>(static_cast<long long>(x)) || x < static_cast<double>(lo) ||
            x > static_cast<double>(hi)) {
            return fail(*v, "\"" + std::string(key) + "\" must be a whole number from " +
                                std::to_string(lo) + " to " + std::to_string(hi));
        }
        out = static_cast<long long>(x);
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

} // namespace owlight::data
