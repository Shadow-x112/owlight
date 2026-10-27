// owlight_sim — run a slime through a scenario and print what the player would have noticed.
//
//   owlight_sim --days 7 --feed charcoal
//   owlight_sim --days 14 --feed iron_scrap --habitat cold_cave --hidden
//   owlight_sim --days 3 --feed grain:1 --feed mushroom:2 --job clean_house --near
//   owlight_sim --data data --days 7 --feed charcoal      (catalog from the data files)
//   owlight_sim --scenario scenarios/first_day.json         (a scripted day-by-day run)
//
// Feedings are spread across the daytime. Day is 06:00–20:00. Everything else is the sim.

#include "owlight/Catalog.h"
#include "owlight/Scenario.h"
#include "owlight/Slime.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace owlight;

namespace {

struct Options {
    int days = 7;
    std::vector<std::pair<std::string, int>> feeds;   // food id, times per day
    std::string habitat = "pen";
    std::string job;
    bool nearPlayer = false;
    bool showHidden = false;
    bool quiet = false;
    unsigned seed = 0;
    std::string dataDir;   // empty = built-in catalog
    std::string scenario;  // a scenario file replaces --days/--feed/--habitat/--job/--near/--seed
    bool list = false;
};

void usage() {
    std::puts(
        "usage: owlight_sim [--days N] [--feed FOOD[:PER_DAY]]... [--habitat ID] [--job ID]\n"
        "                   [--near] [--hidden] [--quiet] [--seed N] [--data DIR] [--list]\n"
        "       owlight_sim --scenario FILE [--hidden] [--quiet] [--data DIR]\n");
}

void listCatalog(const Catalog& c) {
    std::puts("foods:");
    for (const auto& f : c.foods()) std::printf("  %s\n", f.id.c_str());
    std::puts("habitats:");
    for (const auto& h : c.habitats()) std::printf("  %s\n", h.id.c_str());
    std::puts("jobs:");
    for (const auto& j : c.jobs()) std::printf("  %s\n", j.id.c_str());
    std::puts("types (most specific first):");
    for (const auto& t : c.types()) std::printf("  %s%s\n", t.id.c_str(), t.rare ? "  (rare)" : "");
}

void printTime(double hours) {
    const int day = static_cast<int>(hours / 24.0) + 1;
    const int h = static_cast<int>(hours) % 24;
    std::printf("day %2d %02d:00  ", day, h);
}

// Minute precision, for scenarios (a slime found at 14:30 has its journal at :30).
void printClock(double hours) {
    const int minutes = static_cast<int>(std::lround(hours * 60.0));
    std::printf("day %2d %02d:%02d  ", minutes / 1440 + 1, (minutes / 60) % 24, minutes % 60);
}

void printNotice(const Slime& slime, bool showHidden) {
    bool any = false;
    for (const auto& s : slime.signs()) {
        if (!s.text.empty()) { std::printf("  - %s\n", s.text.c_str()); any = true; }
    }
    if (!any) std::puts("  - nothing unusual");
    if (showHidden) {
        std::puts("hidden:");
        for (std::size_t i = 0; i < kPropertyCount; ++i) {
            std::printf("  %-9s %.2f\n", std::string(name(static_cast<Property>(i))).c_str(), slime.values()[i]);
        }
        std::printf("  material  %s\n", std::string(name(slime.affinityMaterial())).c_str());
    }
}

int runScenarioFile(const Options& opt, const Catalog& cat) {
    std::string err;
    const auto sc = Scenario::load(opt.scenario, cat, &err);
    if (!sc) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; }
    const ScenarioRun run = runScenario(*sc, cat);

    if (!opt.quiet) {
        // One journal for the whole household, in world time; file order breaks ties.
        struct Line { double at; std::size_t slime, index; };
        std::vector<Line> lines;
        for (std::size_t i = 0; i < run.slimes.size(); ++i) {
            const auto& entries = run.slimes[i].slime.journal().entries();
            for (std::size_t k = 0; k < entries.size(); ++k)
                lines.push_back({run.slimes[i].bornAt + entries[k].atHours, i, k});
        }
        std::stable_sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.at < b.at; });
        std::puts("journal:");
        for (const auto& l : lines) {
            const auto& s = run.slimes[l.slime];
            const auto& e = s.slime.journal().entries()[l.index];
            printClock(l.at);
            std::printf("%-8s [%s] %s\n", s.id.c_str(), std::string(name(e.kind)).c_str(), e.text.c_str());
        }
        std::puts("");
    }

    std::printf("after %d day(s):\n", sc->days);
    for (const auto& s : run.slimes) {
        std::printf("%s%s%s: %s%s\n", s.id.c_str(), s.slime.name() != s.id ? " (" : "",
                    s.slime.name() != s.id ? (s.slime.name() + ")").c_str() : "",
                    s.slime.type()->id.c_str(), s.slime.locked() ? " (locked in)" : "");
        printNotice(s.slime, opt.showHidden);
    }

    std::puts("");
    for (const auto& f : run.failures) std::printf("FAIL %s\n", f.c_str());
    std::printf("%d expectation(s), %zu failed\n", run.expectations, run.failures.size());
    return run.passed() ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", what); std::exit(2); }
            return argv[++i];
        };
        if (a == "--days") opt.days = std::atoi(next("--days"));
        else if (a == "--feed") {
            std::string spec = next("--feed");
            int per = 3;
            if (auto p = spec.find(':'); p != std::string::npos) { per = std::atoi(spec.c_str() + p + 1); spec.resize(p); }
            opt.feeds.emplace_back(spec, per);
        }
        else if (a == "--habitat") opt.habitat = next("--habitat");
        else if (a == "--job") opt.job = next("--job");
        else if (a == "--near") opt.nearPlayer = true;
        else if (a == "--hidden") opt.showHidden = true;
        else if (a == "--quiet") opt.quiet = true;
        else if (a == "--seed") opt.seed = static_cast<unsigned>(std::atoi(next("--seed")));
        else if (a == "--data") opt.dataDir = next("--data");
        else if (a == "--scenario") opt.scenario = next("--scenario");
        else if (a == "--list") opt.list = true;
        else { usage(); return 2; }
    }

    std::optional<Catalog> loaded;
    if (!opt.dataDir.empty()) {
        std::string err;
        loaded = Catalog::load(opt.dataDir, &err);
        if (!loaded) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; }
    }
    const Catalog& cat = loaded ? *loaded : Catalog::v1();
    if (opt.list) { listCatalog(cat); return 0; }
    if (!opt.scenario.empty()) return runScenarioFile(opt, cat);
    const Habitat* habitat = cat.habitat(opt.habitat);
    if (!habitat) { std::fprintf(stderr, "unknown habitat: %s\n", opt.habitat.c_str()); return 2; }
    const Job* job = nullptr;
    if (!opt.job.empty() && !(job = cat.job(opt.job))) { std::fprintf(stderr, "unknown job: %s\n", opt.job.c_str()); return 2; }
    std::vector<std::pair<const Food*, int>> feeds;
    for (const auto& [id, per] : opt.feeds) {
        const Food* f = cat.food(id);
        if (!f) { std::fprintf(stderr, "unknown food: %s\n", id.c_str()); return 2; }
        feeds.emplace_back(f, per);
    }

    Slime slime("slime", starterValues(opt.seed), cat);
    Context ctx;
    ctx.habitat = habitat;
    ctx.job = job;

    // Schedule: hour-by-hour. Feedings land at evenly spaced daytime hours.
    for (int day = 0; day < opt.days; ++day) {
        for (int hour = 0; hour < 24; ++hour) {
            const bool daytime = hour >= 6 && hour < 20;
            ctx.isDaytime = daytime;
            ctx.nearPlayer = opt.nearPlayer && daytime;
            for (const auto& [food, per] : feeds) {
                for (int k = 0; k < per; ++k) {
                    const int feedHour = 8 + (k * 12) / (per > 1 ? per : 1);   // 08:00 .. 20:00
                    if (feedHour == hour) slime.feed(*food, slime.ageHours());
                }
            }
            slime.tick(1.0, ctx);
        }
    }

    if (!opt.quiet) {
        std::puts("journal:");
        for (const auto& e : slime.journal().entries()) {
            printTime(e.atHours);
            std::printf("[%s] %s\n", std::string(name(e.kind)).c_str(), e.text.c_str());
        }
        std::puts("");
    }

    std::printf("after %d day(s): %s%s\n", opt.days, slime.type()->id.c_str(), slime.locked() ? " (locked in)" : "");
    std::puts("what you'd notice:");
    printNotice(slime, opt.showHidden);
    return 0;
}
