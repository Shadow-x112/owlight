// Scenario files: reading (strict, like the catalog) and replay.

#include "owlight/Scenario.h"

#include "DataReader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>

namespace owlight {

using data::Kind;
using data::Reader;
using data::Value;

namespace {

// "08:00" → 8.0, "14:30" → 14.5.
bool parseClock(Reader& r, const Value& v, double& hours) {
    if (!r.expect(v, Kind::String, "\"at\"")) return false;
    const std::string& s = v.string;
    auto digit = [&](std::size_t i) { return i < s.size() && s[i] >= '0' && s[i] <= '9'; };
    if (s.size() != 5 || !digit(0) || !digit(1) || s[2] != ':' || !digit(3) || !digit(4))
        return r.fail(v, "\"at\" must look like \"08:00\", not \"" + s + "\"");
    const int h = (s[0] - '0') * 10 + (s[1] - '0');
    const int m = (s[3] - '0') * 10 + (s[4] - '0');
    if (h > 23 || m > 59) return r.fail(v, "\"at\" is not a time of day: \"" + s + "\"");
    hours = h + m / 60.0;
    return true;
}

bool knownSlime(const std::vector<std::string>& ids, const std::string& id) {
    for (const auto& s : ids) if (s == id) return true;
    return false;
}

bool readExpectation(Reader& r, const Value& v, Expectation& e) {
    if (!r.expect(v, Kind::Object, "\"expect\"")) return false;
    if (!r.onlyKeys(v, {"type", "locked", "sign", "journal"}, "\"expect\"")) return false;
    if (v.object.empty()) return r.fail(v, "\"expect\" checks nothing");
    if (const Value* t = v.find("type")) {
        if (!r.expect(*t, Kind::String, "\"type\"")) return false;
        e.type = t->string;
    }
    if (v.find("locked")) {
        bool b = false;
        if (!r.boolean(v, "locked", b)) return false;
        e.locked = b;
    }
    for (const char* key : {"sign", "journal"}) {
        if (const Value* s = v.find(key)) {
            if (!r.expect(*s, Kind::String, std::string("\"") + key + "\"")) return false;
            if (s->string.empty()) return r.fail(*s, std::string("\"") + key + "\" is empty");
            (std::string_view(key) == "sign" ? e.sign : e.journal) = s->string;
        }
    }
    return true;
}

bool readScenario(Reader& r, const Value& root, const Catalog& cat, Scenario& sc) {
    if (!r.expect(root, Kind::Object, "the file")) return false;
    if (!r.onlyKeys(root, {"note", "days", "slimes", "script"}, "the scenario")) return false;

    auto habitatOf = [&](const Value& v, const Habitat*& out) {
        if (!r.expect(v, Kind::String, "\"habitat\"")) return false;
        out = cat.habitat(v.string);
        return out ? true : r.fail(v, "unknown habitat \"" + v.string + "\"");
    };

    std::vector<std::string> ids;   // every slime that exists at this point in the script

    // Starter slimes ---------------------------------------------------------------------
    const Value* slimes = root.find("slimes");
    if (!slimes) return r.fail(root, "the scenario needs \"slimes\"");
    if (!r.expect(*slimes, Kind::Array, "\"slimes\"")) return false;
    for (const auto& e : slimes->array) {
        if (!r.expect(e, Kind::Object, "each slime")) return false;
        StarterSlime s;
        if (!r.requiredString(e, "id", s.id, "each slime")) return false;
        if (!r.onlyKeys(e, {"id", "note", "seed", "habitat"}, "slime \"" + s.id + "\"")) return false;
        if (knownSlime(ids, s.id)) return r.fail(*e.find("id"), "duplicate slime \"" + s.id + "\"");
        long long seed = 0;
        if (!r.integer(e, "seed", seed, 0, std::numeric_limits<unsigned>::max())) return false;
        s.seed = static_cast<unsigned>(seed);
        const Value* h = e.find("habitat");
        if (!h) return r.fail(e, "slime \"" + s.id + "\" needs a \"habitat\"");
        if (!habitatOf(*h, s.habitat)) return false;
        ids.push_back(s.id);
        sc.slimes.push_back(std::move(s));
    }

    // The script, day by day --------------------------------------------------------------
    const Value* script = root.find("script");
    if (!script) return r.fail(root, "the scenario needs a \"script\"");
    if (!r.expect(*script, Kind::Array, "\"script\"")) return false;
    double lastAt = 0.0;
    long long lastDay = 0;
    for (const auto& dayV : script->array) {
        if (!r.expect(dayV, Kind::Object, "each day")) return false;
        if (!r.onlyKeys(dayV, {"day", "note", "actions"}, "a day")) return false;
        if (!dayV.find("day")) return r.fail(dayV, "each day needs a \"day\" number");
        long long day = 0;
        if (!r.integer(dayV, "day", day, 1, 100000)) return false;
        if (day <= lastDay) return r.fail(*dayV.find("day"), "days must go forward: day " +
                                          std::to_string(day) + " after day " + std::to_string(lastDay));
        lastDay = day;
        const Value* actions = dayV.find("actions");
        if (!actions) return r.fail(dayV, "day " + std::to_string(day) + " needs \"actions\"");
        if (!r.expect(*actions, Kind::Array, "\"actions\"")) return false;

        for (const auto& a : actions->array) {
            if (!r.expect(a, Kind::Object, "each action")) return false;
            Action act;
            act.line = a.line;

            const Value* at = a.find("at");
            if (!at) return r.fail(a, "each action needs an \"at\" time");
            double clock = 0.0;
            if (!parseClock(r, *at, clock)) return false;
            act.atHours = (day - 1) * 24.0 + clock;
            if (act.atHours < lastAt) return r.fail(*at, "actions must go forward in time");
            lastAt = act.atHours;

            // Exactly one verb.
            static const std::pair<const char*, Action::Verb> kVerbs[] = {
                {"feed", Action::Verb::Feed},     {"move", Action::Verb::Move},
                {"assign", Action::Verb::Assign}, {"pick_up", Action::Verb::PickUp},
                {"name", Action::Verb::Name},     {"near", Action::Verb::Near},
                {"find", Action::Verb::Find},     {"expect", Action::Verb::Expect},
            };
            const Value* verbV = nullptr;
            std::string verbKey;
            for (const auto& [key, verb] : kVerbs) {
                if (const Value* v = a.find(key)) {
                    if (verbV) return r.fail(*v, "one action, one verb: both \"" + verbKey + "\" and \"" + key + "\"");
                    verbV = v; verbKey = key; act.verb = verb;
                }
            }
            if (!verbV) return r.fail(a, "the action needs a verb: feed, move, assign, pick_up, name, near, find or expect");

            if (act.verb == Action::Verb::Find) {
                if (!r.onlyKeys(a, {"at", "note", "find", "seed", "habitat"}, "a find")) return false;
                if (!r.expect(*verbV, Kind::String, "\"find\"")) return false;
                act.slime = verbV->string;
                if (act.slime.empty()) return r.fail(*verbV, "\"find\" needs the new slime's id");
                if (knownSlime(ids, act.slime)) return r.fail(*verbV, "duplicate slime \"" + act.slime + "\"");
                long long seed = 0;
                if (!r.integer(a, "seed", seed, 0, std::numeric_limits<unsigned>::max())) return false;
                act.seed = static_cast<unsigned>(seed);
                const Value* h = a.find("habitat");
                if (!h) return r.fail(a, "a find needs the \"habitat\" it was found in");
                if (!habitatOf(*h, act.habitat)) return false;
                ids.push_back(act.slime);
                sc.actions.push_back(std::move(act));
                continue;
            }

            if (!r.onlyKeys(a, {"at", "note", "slime", verbKey}, "a " + verbKey)) return false;
            if (!r.requiredString(a, "slime", act.slime, "a " + verbKey)) return false;
            if (!knownSlime(ids, act.slime))
                return r.fail(*a.find("slime"), "no slime \"" + act.slime + "\" at this point");

            switch (act.verb) {
                case Action::Verb::Feed:
                    if (!r.expect(*verbV, Kind::String, "\"feed\"")) return false;
                    if (!(act.food = cat.food(verbV->string)))
                        return r.fail(*verbV, "unknown food \"" + verbV->string + "\"");
                    break;
                case Action::Verb::Move:
                    if (!habitatOf(*verbV, act.habitat)) return false;
                    break;
                case Action::Verb::Assign:
                    if (verbV->kind == Kind::Null) break;   // given nothing to do
                    if (!r.expect(*verbV, Kind::String, "\"assign\"")) return false;
                    if (!(act.job = cat.job(verbV->string)))
                        return r.fail(*verbV, "unknown job \"" + verbV->string + "\"");
                    break;
                case Action::Verb::PickUp:
                    if (!r.expect(*verbV, Kind::Bool, "\"pick_up\"")) return false;
                    if (!verbV->boolean) return r.fail(*verbV, "\"pick_up\" must be true");
                    break;
                case Action::Verb::Name:
                    if (!r.expect(*verbV, Kind::String, "\"name\"")) return false;
                    if (verbV->string.empty()) return r.fail(*verbV, "\"name\" is empty");
                    act.text = verbV->string;
                    break;
                case Action::Verb::Near:
                    if (!r.expect(*verbV, Kind::Bool, "\"near\"")) return false;
                    act.near = verbV->boolean;
                    break;
                case Action::Verb::Expect:
                    if (!readExpectation(r, *verbV, act.expect)) return false;
                    if (act.expect.type && !cat.type(*act.expect.type))
                        return r.fail(*verbV->find("type"), "unknown type \"" + *act.expect.type + "\"");
                    break;
                case Action::Verb::Find:
                    break;
            }
            sc.actions.push_back(std::move(act));
        }
    }
    if (script->array.empty()) return r.fail(*script, "the script is empty");

    // Run to the end of the last scripted day unless "days" asks for longer.
    long long days = lastDay;
    if (const Value* d = root.find("days")) {
        if (!r.integer(root, "days", days, 1, 100000)) return false;
        if (days < lastDay) return r.fail(*d, "\"days\" is shorter than the script");
    }
    sc.days = static_cast<int>(days);
    return true;
}

std::string fileName(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string clockText(double worldHours) {
    const int minutes = static_cast<int>(std::lround(worldHours * 60.0));
    const int day = minutes / (24 * 60) + 1;
    const int h = (minutes / 60) % 24, m = minutes % 60;
    char buf[32];
    std::snprintf(buf, sizeof buf, "day %d %02d:%02d", day, h, m);
    return buf;
}

} // namespace

std::optional<Scenario> Scenario::parse(std::string_view text, const std::string& file,
                                        const Catalog& catalog, std::string* error) {
    Scenario sc;
    sc.file = file;
    Reader r(file);
    Value root;
    if (!r.parseText(text, root) || !readScenario(r, root, catalog, sc)) {
        if (error) *error = r.error();
        return std::nullopt;
    }
    if (error) error->clear();
    return sc;
}

std::optional<Scenario> Scenario::load(const std::string& path, const Catalog& catalog,
                                       std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + path;
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return parse(text.str(), fileName(path), catalog, error);
}

const ScenarioSlime* ScenarioRun::find(std::string_view id) const {
    for (const auto& s : slimes) if (s.id == id) return &s;
    return nullptr;
}

ScenarioRun runScenario(const Scenario& sc, const Catalog& catalog, Rates rates) {
    ScenarioRun run;
    auto add = [&](const std::string& id, unsigned seed, const Habitat* habitat, double now) {
        run.slimes.push_back(ScenarioSlime{id, now, Slime(id, starterValues(seed), catalog, rates),
                                           habitat, nullptr, false});
    };
    auto slime = [&](const std::string& id) -> ScenarioSlime& {
        for (auto& s : run.slimes) if (s.id == id) return s;
        return run.slimes.front();   // unreachable: parse() checked every id
    };

    double now = 0.0;
    for (const auto& s : sc.slimes) add(s.id, s.seed, s.habitat, now);

    // Advance the world to `until`, never crossing an hour boundary in one tick, so day and
    // night switch on the hour and replay matches hour-by-hour driving exactly.
    auto advance = [&](double until) {
        while (now < until) {
            const double next = std::min(until, std::floor(now + 1e-9) + 1.0);
            const int hourOfDay = static_cast<int>(std::floor(now + 1e-9)) % 24;
            const bool daytime = hourOfDay >= kDayStartsAt && hourOfDay < kNightStartsAt;
            for (auto& s : run.slimes) {
                Context ctx;
                ctx.habitat = s.habitat;
                ctx.job = s.job;
                ctx.isDaytime = daytime;
                ctx.nearPlayer = s.near && daytime;
                s.slime.tick(next - now, ctx);
            }
            now = next;
        }
    };

    for (const auto& a : sc.actions) {
        advance(a.atHours);
        if (a.verb == Action::Verb::Find) {
            add(a.slime, a.seed, a.habitat, now);
            continue;
        }
        ScenarioSlime& s = slime(a.slime);
        switch (a.verb) {
            case Action::Verb::Feed:   s.slime.feed(*a.food, s.slime.ageHours()); break;
            case Action::Verb::Move:   s.habitat = a.habitat; break;
            case Action::Verb::Assign: s.job = a.job; break;
            case Action::Verb::PickUp: s.slime.pickUp(s.slime.ageHours()); break;
            case Action::Verb::Name:   s.slime.giveName(a.text, s.slime.ageHours()); break;
            case Action::Verb::Near:   s.near = a.near; break;
            case Action::Verb::Find:   break;
            case Action::Verb::Expect: {
                ++run.expectations;
                const Expectation& e = a.expect;
                auto failed = [&](const std::string& what) {
                    run.failures.push_back(sc.file + ":" + std::to_string(a.line) + ": " + clockText(now) +
                                           ", " + s.id + ": " + what);
                };
                if (e.type && s.slime.type()->id != *e.type)
                    failed("expected type " + *e.type + ", found " + s.slime.type()->id);
                if (e.locked && s.slime.locked() != *e.locked)
                    failed(*e.locked ? "expected it locked in, it is not" : "expected it not locked in, it is");
                if (!e.sign.empty()) {
                    bool found = false;
                    for (const auto& sign : s.slime.signs()) found = found || sign.text.find(e.sign) != std::string::npos;
                    if (!found) failed("expected a sign like \"" + e.sign + "\", none showing");
                }
                if (!e.journal.empty()) {
                    bool found = false;
                    for (const auto& en : s.slime.journal().entries()) found = found || en.text.find(e.journal) != std::string::npos;
                    if (!found) failed("expected the journal to mention \"" + e.journal + "\", it does not");
                }
                break;
            }
        }
    }
    advance(sc.days * 24.0);
    return run;
}

} // namespace owlight
