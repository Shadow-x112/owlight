// Tests for the Owlight sim core. No framework: a few macros and a main.
// Each test encodes a design promise from docs/design.md, not an implementation detail.

#include "owlight/Catalog.h"
#include "owlight/Slime.h"

#include <cmath>
#include <cstdio>
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
