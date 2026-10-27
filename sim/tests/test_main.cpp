// Tests for the Owlight sim core. No framework: a few macros and a main.
// Each test encodes a design promise from docs/design.md, not an implementation detail.

#include "owlight/Catalog.h"
#include "owlight/Scenario.h"
#include "owlight/Slime.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

using namespace owlight;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                     \
    do {                                                                                \
        ++g_checks;                                                                     \
        if (!(cond)) {                                                                  \
            ++g_failures;                                                               \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
        }                                                                               \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                              \
    do {                                                                                \
        ++g_checks;                                                                     \
        const std::string _a = (a), _b = (b);                                           \
        if (_a != _b) {                                                                 \
            ++g_failures;                                                               \
            std::printf("  FAIL %s:%d: %s == %s  (\"%s\" vs \"%s\")\n", __FILE__,       \
                        __LINE__, #a, #b, _a.c_str(), _b.c_str());                      \
        }                                                                               \
    } while (0)

struct Test { const char* name; std::function<void()> fn; };
std::vector<Test>& tests() { static std::vector<Test> t; return t; }
struct Register { Register(const char* n, std::function<void()> f) { tests().push_back({n, std::move(f)}); } };
#define TEST(name) \
    static void name(); static Register name##_reg(#name, name); static void name()

PropertyValues rest() { PropertyValues v{}; v.fill(kRestingValue); return v; }

// Run `days` days: feed `food` `perDay` times during daylight, live in `habitat`, work `job`.
void runDays(Slime& s, int days, const Food* food, int perDay, const Habitat* habitat,
             const Job* job = nullptr, bool near = false) {
    Context ctx; ctx.habitat = habitat; ctx.job = job;
    for (int d = 0; d < days; ++d) {
        for (int hour = 0; hour < 24; ++hour) {
            const bool daytime = hour >= 6 && hour < 20;
            ctx.isDaytime = daytime;
            ctx.nearPlayer = near && daytime;
            if (food) {
                for (int k = 0; k < perDay; ++k) {
                    const int feedHour = 8 + (k * 12) / (perDay > 1 ? perDay : 1);
                    if (feedHour == hour) s.feed(*food, s.ageHours());
                }
            }
            s.tick(1.0, ctx);
        }
    }
}

bool journalHas(const Slime& s, EntryKind kind, const std::string& needle) {
    for (const auto& e : s.journal().entries())
        if (e.kind == kind && e.text.find(needle) != std::string::npos) return true;
    return false;
}

} // namespace

// --- The first morning -------------------------------------------------------------------

TEST(plain_slime_stays_plain_when_left_alone) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 7, nullptr, 0, c.habitat("pen"));
    CHECK_EQ_STR(s.type()->id, "plain");
}

TEST(a_day_of_charcoal_makes_a_soot_slime_by_morning) {
    // docs/design.md §8: "The next morning is the payoff."
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 1, c.food("charcoal"), 3, c.habitat("pen"));
    CHECK_EQ_STR(s.type()->id, "plain");          // not yet: it takes the night and one more
    s.feed(*c.food("charcoal"), s.ageHours());    // the morning feeding
    CHECK_EQ_STR(s.type()->id, "soot");
    CHECK(journalHas(s, EntryKind::Became, "soot"));
    // And it would have shown signs before it changed.
    CHECK(journalHas(s, EntryKind::SignChanged, "cloudy") || journalHas(s, EntryKind::SignChanged, "opaque"));
}

TEST(one_accidental_charcoal_is_not_enough) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    s.feed(*c.food("charcoal"), 0.0);
    Context ctx; ctx.habitat = c.habitat("pen");
    s.tick(2.0, ctx);
    CHECK_EQ_STR(s.type()->id, "plain");
}

TEST(seven_days_of_charcoal_is_still_soot) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 7, c.food("charcoal"), 3, c.habitat("pen"));
    CHECK_EQ_STR(s.type()->id, "soot");
}

// --- Consistency -------------------------------------------------------------------------

TEST(same_inputs_same_outcome) {
    const auto& c = Catalog::v1();
    Slime a("a", rest(), c), b("b", rest(), c);
    runDays(a, 5, c.food("iron_scrap"), 3, c.habitat("stove_corner"));
    runDays(b, 5, c.food("iron_scrap"), 3, c.habitat("stove_corner"));
    for (std::size_t i = 0; i < kPropertyCount; ++i) CHECK(std::fabs(a.values()[i] - b.values()[i]) < 1e-12);
    CHECK(a.type() == b.type());
}

TEST(step_size_does_not_change_the_result) {
    // Integrate 48h as one call vs 48 one-hour calls: same place within integration tolerance.
    const auto& c = Catalog::v1();
    Slime a("a", rest(), c), b("b", rest(), c);
    Context ctx; ctx.habitat = c.habitat("cold_cave"); ctx.isDaytime = false;
    a.tick(48.0, ctx);
    for (int i = 0; i < 48; ++i) b.tick(1.0, ctx);
    for (std::size_t i = 0; i < kPropertyCount; ++i) CHECK(std::fabs(a.values()[i] - b.values()[i]) < 1e-9);
}

TEST(starter_variance_does_not_break_the_rules) {
    // Three starters with slightly different hidden values all reach soot on charcoal.
    const auto& c = Catalog::v1();
    for (double jitter : {-0.04, 0.0, 0.04}) {
        PropertyValues v = rest();
        at(v, Property::Clarity) += jitter;
        at(v, Property::Density) -= jitter;
        Slime s("a", v, c);
        runDays(s, 2, c.food("charcoal"), 3, c.habitat("pen"));
        CHECK_EQ_STR(s.type()->id, "soot");
    }
}

// --- Reversal and locking ------------------------------------------------------------------

TEST(stop_feeding_and_soot_drifts_back_to_plain) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 2, c.food("charcoal"), 3, c.habitat("pen"));
    CHECK_EQ_STR(s.type()->id, "soot");
    runDays(s, 14, nullptr, 0, c.habitat("pen"));
    CHECK_EQ_STR(s.type()->id, "plain");
    CHECK(journalHas(s, EntryKind::Reverted, "soot"));
}

TEST(lost_type_lingers_through_the_grace_period) {
    const auto& c = Catalog::v1();
    Rates r; r.revertGraceHours = 12.0;
    Slime s("a", rest(), c, r);
    runDays(s, 2, c.food("charcoal"), 3, c.habitat("pen"));
    CHECK_EQ_STR(s.type()->id, "soot");
    // Push it out of soot instantly, then only a few hours pass.
    s.feed(*c.food("spring_water"), s.ageHours());
    s.feed(*c.food("spring_water"), s.ageHours());
    s.feed(*c.food("spring_water"), s.ageHours());
    s.feed(*c.food("milk"), s.ageHours());
    s.feed(*c.food("milk"), s.ageHours());
    Context ctx; ctx.habitat = c.habitat("pen");
    s.tick(3.0, ctx);
    CHECK_EQ_STR(s.type()->id, "soot");   // still soot: grace
}

TEST(forge_locks_in_after_a_week) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 21, c.food("iron_scrap"), 3, c.habitat("stove_corner"), c.job("forge_assist"));
    CHECK_EQ_STR(s.type()->id, "forge");
    CHECK(s.locked());
    runDays(s, 14, c.food("spring_water"), 3, c.habitat("stream_bank"));
    CHECK_EQ_STR(s.type()->id, "forge");   // locked: cannot be undone
}

// --- Exploration gates rare types ---------------------------------------------------------

TEST(pearl_needs_the_cold_cave) {
    const auto& c = Catalog::v1();
    Slime cave("a", rest(), c), pen("b", rest(), c);
    runDays(cave, 30, c.food("river_stone"), 3, c.habitat("cold_cave"));
    runDays(pen,  30, c.food("river_stone"), 3, c.habitat("pen"));
    CHECK_EQ_STR(cave.type()->id, "pearl");
    CHECK(pen.type()->id != "pearl");
}

TEST(lantern_needs_dark_and_water) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 14, c.food("spring_water"), 3, c.habitat("cave"), c.job("carry_water"));
    CHECK_EQ_STR(s.type()->id, "lantern");
}

TEST(a_brook_slime_becomes_a_lantern_in_the_dark) {
    // Water at the stream makes a brook; move it to the cave and it climbs to lantern.
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 7, c.food("spring_water"), 3, c.habitat("stream_bank"));
    CHECK_EQ_STR(s.type()->id, "brook");
    runDays(s, 10, c.food("spring_water"), 3, c.habitat("cave_pool"));
    CHECK_EQ_STR(s.type()->id, "lantern");
    CHECK(journalHas(s, EntryKind::Became, "lantern"));
}

TEST(a_brook_slime_in_a_dry_cave_loses_its_water) {
    // The lesson the player has to learn: dark is not enough, it needs water near it.
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 7, c.food("spring_water"), 3, c.habitat("stream_bank"));
    CHECK_EQ_STR(s.type()->id, "brook");
    runDays(s, 14, c.food("grain"), 3, c.habitat("cave"));
    CHECK(s.affinityMaterial() == Material::Stone);
    CHECK(s.type()->id != "lantern");
}

TEST(every_type_is_reachable_and_more_specific_types_sort_first) {
    const auto& c = Catalog::v1();
    const auto& ts = c.types();
    for (std::size_t i = 1; i < ts.size(); ++i) CHECK(ts[i - 1].specificity() >= ts[i].specificity());
    CHECK(c.plain().conditions.empty());
}

TEST(materials_compete_for_affinity) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 7, c.food("iron_scrap"), 3, c.habitat("stove_corner"));
    CHECK(s.affinityMaterial() == Material::Metal);
    runDays(s, 21, c.food("spring_water"), 3, c.habitat("stream_bank"), c.job("carry_water"));
    CHECK(s.affinityMaterial() == Material::Water);
}

// --- Life around the slime ----------------------------------------------------------------

TEST(company_makes_it_bold_and_bonded) {
    const auto& c = Catalog::v1();
    Slime with("a", rest(), c), without("b", rest(), c);
    runDays(with,    10, c.food("grain"), 3, c.habitat("pen"), c.job("scout"), /*near=*/true);
    runDays(without, 10, c.food("grain"), 3, c.habitat("pen"), nullptr, /*near=*/false);
    CHECK(at(with.values(), Property::Boldness) > at(without.values(), Property::Boldness));
    CHECK(at(with.values(), Property::Bond) > at(without.values(), Property::Bond));
}

TEST(a_honey_fed_companion_becomes_a_puff) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 10, c.food("honey"), 3, c.habitat("pen"), c.job("scout"), /*near=*/true);
    CHECK_EQ_STR(s.type()->id, "puff");
}

TEST(the_same_job_for_long_enough_makes_it_steady) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 14, c.food("grain"), 3, c.habitat("pen"), c.job("fetch"));
    CHECK(at(s.values(), Property::Memory) > 0.8);
}

TEST(switching_jobs_costs_memory) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    Context ctx; ctx.habitat = c.habitat("pen");
    ctx.job = c.job("fetch");   s.tick(24.0, ctx);
    const double before = at(s.values(), Property::Memory);
    ctx.job = c.job("scout");   s.tick(0.01, ctx);
    CHECK(at(s.values(), Property::Memory) < before);
}

TEST(hunger_shrinks_appetite) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    Context ctx; ctx.habitat = c.habitat("pen");
    s.tick(72.0, ctx);
    CHECK(at(s.values(), Property::Appetite) < kRestingValue);
}

// --- Journal never leaks numbers ------------------------------------------------------------

TEST(journal_text_contains_no_digits) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 10, c.food("charcoal"), 3, c.habitat("stove_corner"), c.job("scrub_stove"), true);
    for (const auto& e : s.journal().entries())
        for (char ch : e.text) CHECK(!(ch >= '0' && ch <= '9'));
    for (const auto& sign : s.signs())
        for (char ch : sign.text) CHECK(!(ch >= '0' && ch <= '9'));
}

TEST(signs_do_not_flicker_on_a_boundary) {
    // Hover a value right on the 0.65 line; the sign must not toggle every step.
    std::array<int, kPropertyCount> bands{};
    int flips = 0, band = 0;
    for (int i = 0; i < 200; ++i) {
        const double x = 0.65 + ((i % 2) ? 0.01 : -0.01);
        bands[0] = band;
        const int next = bandOf(x, band, 0.03);
        if (next != band) ++flips;
        band = next;
    }
    CHECK(flips == 0);
    CHECK(bandOf(0.70, 0, 0.03) == 1);     // a real crossing still registers
    CHECK(bandOf(0.60, 1, 0.03) == 0);     // and so does a real fall
}

TEST(a_week_of_charcoal_writes_a_short_journal) {
    // Noise check: repeated feeding should not spam sign changes.
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 7, c.food("charcoal"), 3, c.habitat("pen"));
    int signChanges = 0;
    for (const auto& e : s.journal().entries()) if (e.kind == EntryKind::SignChanged) ++signChanges;
    CHECK(signChanges <= 8);
}

TEST(signs_change_before_the_type_does) {
    const auto& c = Catalog::v1();
    Slime s("a", rest(), c);
    runDays(s, 2, c.food("charcoal"), 3, c.habitat("pen"));
    double firstSign = 1e9, became = -1;
    for (const auto& e : s.journal().entries()) {
        if (e.kind == EntryKind::SignChanged && e.atHours < firstSign) firstSign = e.atHours;
        if (e.kind == EntryKind::Became && became < 0) became = e.atHours;
    }
    CHECK(became >= 0);
    CHECK(firstSign <= became);
}

// --- The catalog as data ------------------------------------------------------------------

namespace {

// Report which entry differs, so a mismatch points at the line to fix.
template <class T>
void checkSameEntries(const char* what, const std::vector<T>& loaded, const std::vector<T>& builtIn) {
    CHECK(loaded.size() == builtIn.size());
    for (std::size_t i = 0; i < loaded.size() && i < builtIn.size(); ++i) {
        if (loaded[i] != builtIn[i])
            std::printf("  %s #%zu differs: \"%s\" (data) vs \"%s\" (built in)\n", what, i,
                        loaded[i].id.c_str(), builtIn[i].id.c_str());
        CHECK(loaded[i] == builtIn[i]);
    }
}

// A copy of data/ to break on purpose. Returns the directory.
std::string scratchCopyOfData(const char* name) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(OWLIGHT_TEST_SCRATCH) / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    for (const char* f : {"foods.json", "habitats.json", "jobs.json", "types.json"})
        fs::copy_file(fs::path(OWLIGHT_DATA_DIR) / f, dir / f);
    return dir.string();
}

void replaceInFile(const std::string& path, const std::string& from, const std::string& to) {
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    const auto pos = text.find(from);
    CHECK(pos != std::string::npos);
    if (pos != std::string::npos) text.replace(pos, from.size(), to);
    std::ofstream(path, std::ios::binary) << text;
}

} // namespace

TEST(loaded_catalog_equals_the_built_in_one) {
    // The numbers must not change by moving them into data/: exact equality, every field.
    std::string err;
    const auto loaded = Catalog::load(OWLIGHT_DATA_DIR, &err);
    if (!loaded) std::printf("  load failed: %s\n", err.c_str());
    CHECK(loaded.has_value());
    if (!loaded) return;
    const auto& b = Catalog::v1();
    checkSameEntries("food", loaded->foods(), b.foods());
    checkSameEntries("habitat", loaded->habitats(), b.habitats());
    checkSameEntries("job", loaded->jobs(), b.jobs());
    checkSameEntries("type", loaded->types(), b.types());
    CHECK(*loaded == b);
    CHECK_EQ_STR(loaded->plain().id, "plain");
}

TEST(the_first_morning_plays_the_same_from_data) {
    const auto loaded = Catalog::load(OWLIGHT_DATA_DIR);
    CHECK(loaded.has_value());
    if (!loaded) return;
    const auto& c = *loaded;
    Slime fromData("a", rest(), c), builtIn("b", rest(), Catalog::v1());
    runDays(fromData, 1, c.food("charcoal"), 3, c.habitat("pen"));
    runDays(builtIn, 1, Catalog::v1().food("charcoal"), 3, Catalog::v1().habitat("pen"));
    fromData.feed(*c.food("charcoal"), fromData.ageHours());
    builtIn.feed(*Catalog::v1().food("charcoal"), builtIn.ageHours());
    CHECK_EQ_STR(fromData.type()->id, "soot");
    CHECK(fromData.values() == builtIn.values());
    CHECK(fromData.journal().entries().size() == builtIn.journal().entries().size());
}

TEST(a_copied_catalog_still_resolves_types) {
    // plain() is an index, not a pointer, so copies stay valid after the original is gone.
    std::optional<Catalog> copy;
    {
        const auto loaded = Catalog::load(OWLIGHT_DATA_DIR);
        CHECK(loaded.has_value());
        if (!loaded) return;
        copy = *loaded;
    }
    CHECK_EQ_STR(copy->plain().id, "plain");
    PropertyValues v = rest();
    CHECK_EQ_STR(copy->resolve(v, Material::None)->id, "plain");
}

TEST(a_typo_in_the_data_names_the_file_and_line) {
    // Tuning is done by hand; a misspelt property must fail loudly, never load as zero.
    const std::string dir = scratchCopyOfData("typo");
    replaceInFile(dir + "/types.json", "\"density\": { \"min\": 0.70 }", "\"densty\": { \"min\": 0.70 }");
    std::string err;
    CHECK(!Catalog::load(dir, &err).has_value());
    CHECK(err.find("types.json:3:") != std::string::npos);
    CHECK(err.find("densty") != std::string::npos);
    if (err.find("types.json:3:") == std::string::npos) std::printf("  error was: %s\n", err.c_str());
}

TEST(bad_data_is_rejected_with_a_reason) {
    struct Case { const char* file; const char* from; const char* to; const char* expect; };
    const Case cases[] = {
        {"foods.json", "\"id\": \"grain\", \"per_feeding\"", "\"id\": \"grain\", \"per_feding\"", "unknown key \"per_feding\""},
        {"foods.json", "\"material\": \"metal\"", "\"material\": \"iron\"", "unknown material \"iron\""},
        {"foods.json", "\"id\": \"mushroom\"", "\"id\": \"grain\"", "duplicate id \"grain\""},
        {"habitats.json", "\"daylight\": 0.6", "\"daylight\": 1.6", "\"daylight\" must be between"},
        {"habitats.json", "\"loud\": true", "\"loud\": 1", "\"loud\" must be a boolean"},
        {"jobs.json", "\"scout\", \"per_hour\"", "\"scout\" \"per_hour\"", "expected ',' or '}'"},
        {"types.json", "{ \"id\": \"plain\", \"conditions\": {} },", "", "type with id \"plain\""},
        {"types.json", "\"acidity\": { \"min\": 0.55, \"max\": 0.80 }", "\"acidity\": { \"min\": 0.85, \"max\": 0.80 }", "min above max"},
        {"types.json", "\"material\": \"water\"", "\"material\": \"none\"", "unknown material \"none\""},
    };
    int n = 0;
    for (const auto& k : cases) {
        const std::string dir = scratchCopyOfData(("bad" + std::to_string(n++)).c_str());
        replaceInFile(dir + "/" + k.file, k.from, k.to);
        std::string err;
        const bool loaded = Catalog::load(dir, &err).has_value();
        const bool ok = !loaded && err.find(k.file) == 0 && err.find(k.expect) != std::string::npos;
        if (!ok) std::printf("  %s: expected \"%s\", got \"%s\"\n", k.file, k.expect, err.c_str());
        CHECK(ok);
    }
    std::string err;
    CHECK(!Catalog::load(std::string(OWLIGHT_TEST_SCRATCH) + "/no_such_dir", &err).has_value());
    CHECK(err.find("cannot open") != std::string::npos);
}

// --- Scenario files ------------------------------------------------------------------------

namespace {

std::optional<ScenarioRun> runText(const std::string& text, std::string* err = nullptr) {
    const auto sc = Scenario::parse(text, "test.json", Catalog::v1(), err);
    if (!sc) return std::nullopt;
    return runScenario(*sc, Catalog::v1());
}

} // namespace

TEST(the_first_day_replays_from_its_scenario_file) {
    // docs/design.md §8, scripted in scenarios/first_day.json, and its expectations hold.
    const auto& c = Catalog::v1();
    std::string err;
    const auto sc = Scenario::load(std::string(OWLIGHT_SCENARIO_DIR) + "/first_day.json", c, &err);
    if (!sc) std::printf("  load failed: %s\n", err.c_str());
    CHECK(sc.has_value());
    if (!sc) return;
    const ScenarioRun run = runScenario(*sc, c);
    for (const auto& f : run.failures) std::printf("  %s\n", f.c_str());
    CHECK(run.passed());
    CHECK(run.expectations >= 10);

    // The payoff lands on the morning feeding of day 2, not a moment before.
    const ScenarioSlime* inky = run.find("inky");
    CHECK(inky != nullptr);
    if (!inky) return;
    double became = -1.0;
    for (const auto& e : inky->slime.journal().entries())
        if (e.kind == EntryKind::Became && became < 0) became = inky->bornAt + e.atHours;
    CHECK(became == 24.0 + 8.0);
    CHECK_EQ_STR(inky->slime.name(), "Sooty");

    // The slime found under the rock joined the household in the afternoon and lives in the pen.
    const ScenarioSlime* pebble = run.find("pebble");
    CHECK(pebble != nullptr);
    if (!pebble) return;
    CHECK(pebble->bornAt == 14.0);
    CHECK_EQ_STR(pebble->habitat->id, "pen");
    CHECK(run.slimes.size() == 4);

    // Picking it up by hand bonded it: the same afternoon without the pick-up leaves less bond.
    std::ifstream in(std::string(OWLIGHT_SCENARIO_DIR) + "/first_day.json", std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string pickUp = R"({ "at": "14:00", "slime": "pebble", "pick_up": true, "note": "by hand" },)";
    CHECK(text.find(pickUp) != std::string::npos);
    if (text.find(pickUp) == std::string::npos) return;
    text.erase(text.find(pickUp), pickUp.size());
    const auto untouched = Scenario::parse(text, "first_day.json", c);
    CHECK(untouched.has_value());
    if (!untouched) return;
    const ScenarioRun other = runScenario(*untouched, c);
    CHECK(at(pebble->slime.values(), Property::Bond) > at(other.find("pebble")->slime.values(), Property::Bond));
}

TEST(a_scenario_replays_exactly_like_hour_by_hour_driving) {
    // The runner splits time on the hour, so it matches runDays to the last bit.
    const auto& c = Catalog::v1();
    const auto run = runText(R"({
      "days": 3,
      "slimes": [ { "id": "a", "habitat": "stove_corner" } ],
      "script": [
        { "day": 1, "actions": [
          { "at": "00:00", "slime": "a", "assign": "scrub_stove" },
          { "at": "00:00", "slime": "a", "near": true },
          { "at": "08:00", "slime": "a", "feed": "charcoal" },
          { "at": "12:00", "slime": "a", "feed": "charcoal" },
          { "at": "16:00", "slime": "a", "feed": "charcoal" } ]},
        { "day": 2, "actions": [
          { "at": "08:00", "slime": "a", "feed": "charcoal" },
          { "at": "12:00", "slime": "a", "feed": "charcoal" },
          { "at": "16:00", "slime": "a", "feed": "charcoal" } ]},
        { "day": 3, "actions": [
          { "at": "08:00", "slime": "a", "feed": "charcoal" },
          { "at": "12:00", "slime": "a", "feed": "charcoal" },
          { "at": "16:00", "slime": "a", "feed": "charcoal" } ]}
      ]
    })");
    CHECK(run.has_value());
    if (!run) return;
    Slime s("a", rest(), c);
    runDays(s, 3, c.food("charcoal"), 3, c.habitat("stove_corner"), c.job("scrub_stove"), /*near=*/true);
    const Slime& replayed = run->slimes.front().slime;
    CHECK(replayed.values() == s.values());
    CHECK_EQ_STR(replayed.type()->id, s.type()->id);
    CHECK(replayed.journal().entries().size() == s.journal().entries().size());
}

TEST(a_failed_expectation_says_where_and_what) {
    std::string err;
    const auto run = runText(R"({
      "slimes": [ { "id": "a", "habitat": "pen" } ],
      "script": [ { "day": 1, "actions": [
        { "at": "09:00", "slime": "a", "feed": "charcoal" },
        { "at": "09:30", "slime": "a",
          "expect": { "type": "soot" } }
      ]} ]
    })", &err);
    CHECK(run.has_value());
    if (!run) { std::printf("  %s\n", err.c_str()); return; }
    CHECK(!run->passed());
    CHECK(run->failures.size() == 1);
    if (run->failures.size() != 1) return;
    const std::string& f = run->failures.front();
    CHECK(f.find("test.json:5:") == 0);
    CHECK(f.find("day 1 09:30") != std::string::npos);
    CHECK(f.find("expected type soot, found plain") != std::string::npos);
    if (f.find("test.json:5:") != 0) std::printf("  failure was: %s\n", f.c_str());
}

TEST(bad_scenarios_are_rejected_with_a_reason) {
    const std::string head = R"({ "slimes": [ { "id": "a", "habitat": "pen" } ], "script": [ { "day": 1, "actions": [ )";
    const std::string tail = R"( ] } ] })";
    struct Case { const char* actions; const char* expect; };
    const Case cases[] = {
        {R"({ "at": "08:00", "slime": "a", "feed": "charcol" })", "unknown food \"charcol\""},
        {R"({ "at": "08:00", "slime": "b", "feed": "grain" })", "no slime \"b\" at this point"},
        {R"({ "at": "08:00", "slime": "a", "feed": "grain", "move": "cave" })", "one action, one verb"},
        {R"({ "at": "8:00", "slime": "a", "feed": "grain" })", "must look like \"08:00\""},
        {R"({ "at": "25:00", "slime": "a", "feed": "grain" })", "not a time of day"},
        {R"({ "at": "09:00", "slime": "a", "feed": "grain" }, { "at": "08:00", "slime": "a", "feed": "grain" })", "go forward in time"},
        {R"({ "at": "08:00", "slime": "a", "move": "attic" })", "unknown habitat \"attic\""},
        {R"({ "at": "08:00", "slime": "a", "assign": "dance" })", "unknown job \"dance\""},
        {R"({ "at": "08:00", "slime": "a", "expect": { "type": "dragon" } })", "unknown type \"dragon\""},
        {R"({ "at": "08:00", "slime": "a", "expect": { "colour": "blue" } })", "unknown key \"colour\""},
        {R"({ "at": "08:00", "slime": "a" })", "needs a verb"},
        {R"({ "at": "08:00", "find": "a", "habitat": "cave" })", "duplicate slime \"a\""},
        {R"({ "at": "08:00", "slime": "a", "feed": "grain", "seed": 3 })", "unknown key \"seed\""},
    };
    for (const auto& k : cases) {
        std::string err;
        const bool parsed = runText(head + k.actions + tail, &err).has_value();
        const bool ok = !parsed && err.find("test.json:1:") == 0 && err.find(k.expect) != std::string::npos;
        if (!ok) std::printf("  expected \"%s\", got \"%s\"\n", k.expect, err.c_str());
        CHECK(ok);
    }
    std::string err;
    CHECK(!Scenario::load("no_such_scenario.json", Catalog::v1(), &err).has_value());
    CHECK(err.find("cannot open") != std::string::npos);
}

TEST(the_first_day_plays_the_same_on_the_data_catalog) {
    const std::string file = std::string(OWLIGHT_SCENARIO_DIR) + "/first_day.json";
    const auto loaded = Catalog::load(OWLIGHT_DATA_DIR);
    CHECK(loaded.has_value());
    if (!loaded) return;
    const auto onData = Scenario::load(file, *loaded);
    const auto builtIn = Scenario::load(file, Catalog::v1());
    CHECK(onData.has_value() && builtIn.has_value());
    if (!onData || !builtIn) return;
    const ScenarioRun a = runScenario(*onData, *loaded);
    const ScenarioRun b = runScenario(*builtIn, Catalog::v1());
    CHECK(a.passed());
    CHECK(a.slimes.size() == b.slimes.size());
    for (std::size_t i = 0; i < a.slimes.size() && i < b.slimes.size(); ++i)
        CHECK(a.slimes[i].slime.values() == b.slimes[i].slime.values());
}

int main() {
    int failedTests = 0;
    for (const auto& t : tests()) {
        const int before = g_failures;
        t.fn();
        const bool ok = g_failures == before;
        if (!ok) ++failedTests;
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", t.name);
    }
    std::printf("\n%d checks, %d failures, %zu tests, %d failed\n", g_checks, g_failures, tests().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
