// owlight_sim — run a slime through a scenario and print what the player would have noticed.
//
//   owlight_sim --days 7 --feed charcoal
//   owlight_sim --days 14 --feed iron_scrap --habitat cold_cave --hidden
//   owlight_sim --days 3 --feed grain:1 --feed mushroom:2 --job clean_house --near
//
// Feedings are spread across the daytime. Day is 06:00–20:00. Everything else is the sim.

#include "owlight/Catalog.h"
#include "owlight/Slime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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
};

void usage() {
    std::puts(
        "usage: owlight_sim [--days N] [--feed FOOD[:PER_DAY]]... [--habitat ID] [--job ID]\n"
        "                   [--near] [--hidden] [--quiet] [--seed N] [--list]\n");
}

void listCatalog() {
    const auto& c = Catalog::v1();
    std::puts("foods:");
    for (const auto& f : c.foods()) std::printf("  %s\n", f.id.c_str());
    std::puts("habitats:");
    for (const auto& h : c.habitats()) std::printf("  %s\n", h.id.c_str());
    std::puts("jobs:");
    for (const auto& j : c.jobs()) std::printf("  %s\n", j.id.c_str());
    std::puts("types (most specific first):");
    for (const auto& t : c.types()) std::printf("  %s%s\n", t.id.c_str(), t.rare ? "  (rare)" : "");
}

// Small deterministic variance so starter slimes differ a little. Not real randomness:
// the same seed always gives the same slime.
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

void printTime(double hours) {
    const int day = static_cast<int>(hours / 24.0) + 1;
    const int h = static_cast<int>(hours) % 24;
    std::printf("day %2d %02d:00  ", day, h);
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
        else if (a == "--list") { listCatalog(); return 0; }
        else { usage(); return 2; }
    }

    const auto& cat = Catalog::v1();
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
    bool any = false;
    for (const auto& s : slime.signs()) {
        if (!s.text.empty()) { std::printf("  - %s\n", s.text.c_str()); any = true; }
    }
    if (!any) std::puts("  - nothing unusual");

    if (opt.showHidden) {
        std::puts("hidden:");
        for (std::size_t i = 0; i < kPropertyCount; ++i) {
            std::printf("  %-9s %.2f\n", std::string(name(static_cast<Property>(i))).c_str(), slime.values()[i]);
        }
        std::printf("  material  %s\n", std::string(name(slime.affinityMaterial())).c_str());
    }
    return 0;
}
