#include "owlight/Scenario.h"

#include "Json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace owlight {

namespace {

using json::Value;
using Kind = json::Value::Kind;

bool isDaytime(double hours) {
    const int h = static_cast<int>(std::floor(std::fmod(hours, 24.0)));
    return h >= 6 && h < 20;
}

// "day 1 08:00" -> 8.0 ; "day 2 09:30" -> 33.5 ; a bare number is hours.
bool parseTime(const Value& v, double& out, std::string& why) {
    if (v.kind == Kind::Number) {
        if (v.number < 0.0) { why = "time cannot be negative"; return false; }
        out = v.number;
        return true;
    }
    if (v.kind != Kind::String) { why = "time must be \"day D HH:MM\" or a number of hours"; return false; }
    int day = 0, hh = 0, mm = 0;
    char tail = 0;
    if (std::sscanf(v.string.c_str(), "day %d %d:%d%c", &day, &hh, &mm, &tail) != 3 ||
        day < 1 || hh < 0 || hh > 23 || mm < 0 || mm > 59) {
        why = "time must be \"day D HH:MM\" (day 1, 00:00–23:59) or a number of hours";
        return false;
    }
    out = (day - 1) * 24.0 + hh + mm / 60.0;
    return true;
}

std::string fmtTime(double hours) {
    const int day = static_cast<int>(hours / 24.0) + 1;
    const double rem = hours - (day - 1) * 24.0;
    const int hh = static_cast<int>(rem);
    const int mm = static_cast<int>(std::lround((rem - hh) * 60.0));
    char buf[32];
    std::snprintf(buf, sizeof buf, "day %d %02d:%02d", day, hh, mm);
    return buf;
}

} // namespace

PropertyValues starterValues(unsigned seed) {
    PropertyValues v{};
    unsigned x = seed * 2654435761u + 12345u;
    for (auto& p : v) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        const double jitter = seed == 0 ? 0.0 : (static_cast<double>(x % 1000) / 1000.0 - 0.5) * 0.10;
        p = kRestingValue + jitter;
    }
    return v;
}

std::optional<Scenario> Scenario::loadFile(const std::string& path, const Catalog& catalog, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + path;
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    auto s = parse(text.str(), catalog, error);
    if (!s && error) *error = path + ":" + *error;
    return s;
}

std::optional<Scenario> Scenario::parse(const std::string& text, const Catalog& catalog, std::string* error) {
    auto fail = [&](int line, const std::string& msg) -> std::optional<Scenario> {
        if (error) *error = std::to_string(line) + ": " + msg;
        return std::nullopt;
    };

    Value root;
    json::Error err;
    if (!json::parse(text, root, err)) return fail(err.line, err.message);
    if (root.kind != Kind::Object) return fail(root.line, "the scenario must be an object");

    for (const auto& m : root.object) {
        static const char* allowed[] = {"name", "seed", "habitat", "end", "steps"};
        if (std::find(std::begin(allowed), std::end(allowed), m.key) == std::end(allowed))
            return fail(m.value.line, "unknown key \"" + m.key + "\"");
    }

    Scenario s;
    if (const Value* v = root.find("name")) {
        if (v->kind != Kind::String) return fail(v->line, "\"name\" must be a string");
        s.name = v->string;
    }
    if (const Value* v = root.find("seed")) {
        if (v->kind != Kind::Number || v->number < 0 || v->number != std::floor(v->number))
            return fail(v->line, "\"seed\" must be a whole number >= 0");
        s.seed = static_cast<unsigned>(v->number);
    }
    if (const Value* v = root.find("habitat")) {
        if (v->kind != Kind::String) return fail(v->line, "\"habitat\" must be a string");
        if (!catalog.habitat(v->string)) return fail(v->line, "unknown habitat \"" + v->string + "\"");
        s.habitat = v->string;
    }
    const Value* endV = root.find("end");
    if (!endV) return fail(root.line, "the scenario needs an \"end\" time");
    {
        std::string why;
        if (!parseTime(*endV, s.endHours, why)) return fail(endV->line, "\"end\": " + why);
    }

    const Value* stepsV = root.find("steps");
    if (!stepsV) return fail(root.line, "the scenario needs \"steps\"");
    if (stepsV->kind != Kind::Array) return fail(stepsV->line, "\"steps\" must be an array");

    for (const auto& e : stepsV->array) {
        if (e.kind != Kind::Object) return fail(e.line, "each step must be an object");
        const Value* atV = e.find("at");
        if (!atV) return fail(e.line, "each step needs \"at\"");
        Step st;
        st.line = e.line;
        {
            std::string why;
            if (!parseTime(*atV, st.atHours, why)) return fail(atV->line, "\"at\": " + why);
        }
        if (st.atHours > s.endHours) return fail(atV->line, "step is after \"end\"");

        int actions = 0;
        for (const auto& m : e.object) {
            const Value& v = m.value;
            if (m.key == "at") continue;
            ++actions;
            if (m.key == "feed") {
                if (v.kind != Kind::String) return fail(v.line, "\"feed\" must be a food id");
                if (!catalog.food(v.string)) return fail(v.line, "unknown food \"" + v.string + "\"");
                st.kind = Step::Kind::Feed; st.arg = v.string;
            } else if (m.key == "pick_up") {
                if (v.kind != Kind::Bool || !v.boolean) return fail(v.line, "\"pick_up\" must be true");
                st.kind = Step::Kind::PickUp;
            } else if (m.key == "name") {
                if (v.kind != Kind::String || v.string.empty()) return fail(v.line, "\"name\" must be a non-empty string");
                st.kind = Step::Kind::Name; st.arg = v.string;
            } else if (m.key == "move") {
                if (v.kind != Kind::String) return fail(v.line, "\"move\" must be a habitat id");
                if (!catalog.habitat(v.string)) return fail(v.line, "unknown habitat \"" + v.string + "\"");
                st.kind = Step::Kind::Move; st.arg = v.string;
            } else if (m.key == "assign") {
                if (v.kind == Kind::Null) { st.kind = Step::Kind::Assign; st.flag = true; }
                else {
                    if (v.kind != Kind::String) return fail(v.line, "\"assign\" must be a job id or null");
                    if (!catalog.job(v.string)) return fail(v.line, "unknown job \"" + v.string + "\"");
                    st.kind = Step::Kind::Assign; st.arg = v.string;
                }
            } else if (m.key == "near") {
                if (v.kind != Kind::Bool) return fail(v.line, "\"near\" must be true or false");
                st.kind = Step::Kind::Near; st.flag = v.boolean;
            } else if (m.key == "expect") {
                if (v.kind != Kind::Object) return fail(v.line, "\"expect\" must be an object");
                st.kind = Step::Kind::Expect;
                for (const auto& x : v.object) {
                    if (x.key == "type") {
                        if (x.value.kind != Kind::String) return fail(x.value.line, "\"type\" must be a type id");
                        if (!catalog.type(x.value.string)) return fail(x.value.line, "unknown type \"" + x.value.string + "\"");
                        st.expect.type = x.value.string;
                    } else if (x.key == "locked") {
                        if (x.value.kind != Kind::Bool) return fail(x.value.line, "\"locked\" must be true or false");
                        st.expect.locked = x.value.boolean;
                    } else if (x.key == "sign") {
                        if (x.value.kind != Kind::String || x.value.string.empty()) return fail(x.value.line, "\"sign\" must be a non-empty string");
                        st.expect.sign = x.value.string;
                    } else {
                        return fail(x.value.line, "unknown key \"" + x.key + "\" in \"expect\"");
                    }
                }
                if (!st.expect.type && !st.expect.locked && !st.expect.sign)
                    return fail(v.line, "\"expect\" needs \"type\", \"locked\" or \"sign\"");
            } else {
                return fail(v.line, "unknown key \"" + m.key + "\" in step");
            }
        }
        if (actions != 1) return fail(e.line, "each step must have exactly one action");
        s.steps.push_back(std::move(st));
    }

    std::stable_sort(s.steps.begin(), s.steps.end(),
                     [](const Step& a, const Step& b) { return a.atHours < b.atHours; });
    if (error) error->clear();
    return s;
}

Slime makeScenarioSlime(const Scenario& scenario, const Catalog& catalog, Rates rates) {
    return Slime("slime", starterValues(scenario.seed), catalog, rates);
}

ScenarioResult runScenario(const Scenario& scenario, const Catalog& catalog, Slime& slime) {
    ScenarioResult result;
    Context ctx;
    ctx.habitat = catalog.habitat(scenario.habitat);
    bool near = false;

    auto advanceTo = [&](double target) {
        // Tick in whole-hour steps so day/night flips land on the hour.
        while (slime.ageHours() + 1e-9 < target) {
            const double now = slime.ageHours();
            const double nextHour = std::floor(now + 1e-9) + 1.0;
            const double h = std::min(target, nextHour) - now;
            ctx.isDaytime = isDaytime(now);
            ctx.nearPlayer = near && ctx.isDaytime;
            slime.tick(h, ctx);
        }
    };

    for (const auto& st : scenario.steps) {
        advanceTo(st.atHours);
        const double t = slime.ageHours();
        switch (st.kind) {
            case Step::Kind::Feed:   slime.feed(*catalog.food(st.arg), t); break;
            case Step::Kind::PickUp: slime.pickUp(t); break;
            case Step::Kind::Name:   slime.giveName(st.arg, t); break;
            case Step::Kind::Move:   ctx.habitat = catalog.habitat(st.arg); break;
            case Step::Kind::Assign: ctx.job = st.flag ? nullptr : catalog.job(st.arg); break;
            case Step::Kind::Near:   near = st.flag; break;
            case Step::Kind::Expect: {
                auto failed = [&](const std::string& msg) {
                    result.failures.push_back(Failure{t, st.line, fmtTime(t) + ": " + msg});
                };
                if (st.expect.type && slime.type()->id != *st.expect.type)
                    failed("expected a " + *st.expect.type + ", it is a " + slime.type()->id);
                if (st.expect.locked && slime.locked() != *st.expect.locked)
                    failed(std::string("expected it to be ") + (*st.expect.locked ? "locked in" : "not locked in"));
                if (st.expect.sign) {
                    bool found = false;
                    for (const auto& sg : slime.signs())
                        if (sg.text.find(*st.expect.sign) != std::string::npos) found = true;
                    if (!found) failed("expected a sign containing \"" + *st.expect.sign + "\"");
                }
                break;
            }
        }
    }
    advanceTo(scenario.endHours);
    return result;
}

} // namespace owlight
