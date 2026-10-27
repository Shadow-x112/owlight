#include "owlight/Journal.h"

namespace owlight {

int bandOf(double x) { return bandOf(x, 0, 0.0); }

int bandOf(double x, int currentBand, double hysteresis) {
    // Boundaries between bands (-2|-1), (-1|0), (0|1), (1|2).
    static constexpr double kBounds[4] = {0.20, 0.35, 0.65, 0.80};
    int crossed = 0;
    for (int i = 0; i < 4; ++i) {
        const int bandBelow = i - 2;              // band just under this boundary
        // Rising across the boundary needs a margin; so does falling back under it.
        const double edge = currentBand <= bandBelow ? kBounds[i] + hysteresis : kBounds[i] - hysteresis;
        if (x >= edge) ++crossed;
    }
    return crossed - 2;
}

namespace {

// What the player would notice, in plain words. Never a number.
const char* signText(Property p, int band) {
    switch (p) {
        case Property::Density:
            if (band == -2) return "so light it barely dents the straw";
            if (band == -1) return "sits lightly, almost bobbing";
            if (band ==  1) return "sits heavy where it rests";
            if (band ==  2) return "sinks deep into whatever it rests on";
            break;
        case Property::Heat:
            if (band == -2) return "cold to the touch, beaded with dew";
            if (band == -1) return "cool; dew gathers on it at night";
            if (band ==  1) return "warm; the straw under it is dry";
            if (band ==  2) return "gives off a faint glow in the dark";
            break;
        case Property::Acidity:
            if (band == -2) return "the moss under it grows thicker";
            if (band == -1) return "leaves a damp, clean patch where it sat";
            if (band ==  1) return "the grass under it has yellowed";
            if (band ==  2) return "it leaves a bare, bleached ring behind";
            break;
        case Property::Clarity:
            if (band == -2) return "dull and opaque, like wet clay";
            if (band == -1) return "cloudy; you can't see into it";
            if (band ==  1) return "you can see light through it";
            if (band ==  2) return "clear as a bead of water";
            break;
        case Property::Appetite:
            if (band == -2) return "ignores food left beside it";
            if (band == -1) return "picks at food slowly";
            if (band ==  1) return "hurries toward anything you carry";
            if (band ==  2) return "noticeably bigger, and never full";
            break;
        case Property::Boldness:
            if (band == -2) return "hides the moment it sees you";
            if (band == -1) return "keeps its distance from you";
            if (band ==  1) return "comes right up to your feet";
            if (band ==  2) return "tries to climb your boot";
            break;
        case Property::Memory:
            if (band == -2) return "forgets what it was doing mid-task";
            if (band == -1) return "needs showing each time";
            if (band ==  1) return "starts its task before you ask";
            if (band ==  2) return "finishes whole jobs on its own";
            break;
        case Property::Affinity:
            if (band ==  1) return "drifts toward the same thing whenever it can";
            if (band ==  2) return "won't leave its favorite material alone";
            break;
        case Property::Light:
            if (band == -2) return "only moves after dark";
            if (band == -1) return "sluggish in daylight";
            if (band ==  1) return "sleeps as soon as the sun sets";
            if (band ==  2) return "follows patches of sunlight around";
            break;
        case Property::Bond:
            if (band == -2) return "shrinks away when you reach for it";
            if (band == -1) return "tolerates being picked up";
            if (band ==  1) return "follows you as far as it can";
            if (band ==  2) return "follows you everywhere you let it";
            break;
        case Property::Count:
            break;
    }
    return "";
}

} // namespace

std::vector<Sign> signsFor(const PropertyValues& values, const std::array<int, kPropertyCount>& currentBands,
                           double hysteresis) {
    std::vector<Sign> out;
    out.reserve(kPropertyCount);
    for (std::size_t i = 0; i < kPropertyCount; ++i) {
        const auto p = static_cast<Property>(i);
        const int band = bandOf(values[i], currentBands[i], hysteresis);
        out.push_back(Sign{p, band, signText(p, band)});
    }
    return out;
}

std::vector<Sign> signsFor(const PropertyValues& values) {
    std::array<int, kPropertyCount> none{};
    return signsFor(values, none, 0.0);
}

void Journal::add(double atHours, EntryKind kind, std::string text) {
    entries_.push_back(Entry{atHours, kind, std::move(text)});
}

std::string_view name(EntryKind k) {
    switch (k) {
        case EntryKind::Fed:         return "fed";
        case EntryKind::Moved:       return "moved";
        case EntryKind::Assigned:    return "assigned";
        case EntryKind::SignChanged: return "sign";
        case EntryKind::Stirring:    return "stirring";
        case EntryKind::Became:      return "became";
        case EntryKind::Reverted:    return "reverted";
        case EntryKind::Locked:      return "locked";
        case EntryKind::Note:        return "note";
    }
    return "?";
}

std::optional<EntryKind> entryKindFromName(std::string_view s) {
    for (int i = 0; i <= static_cast<int>(EntryKind::Note); ++i) {
        const auto k = static_cast<EntryKind>(i);
        if (name(k) == s) return k;
    }
    return std::nullopt;
}

} // namespace owlight
