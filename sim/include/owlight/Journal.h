#pragma once
// The player-facing side of the simulation: what a slime *looks like* it is doing,
// and a log of the moments worth writing down. Never exposes numbers.

#include "owlight/Properties.h"

#include <optional>
#include <string>
#include <vector>

namespace owlight {

// A visible sign derived from one property, e.g. "sits heavy in the pen".
struct Sign {
    Property property;
    int band;             // -2 very low, -1 low, 0 unremarkable, 1 high, 2 very high
    std::string text;     // empty when band == 0
};

// Band boundaries for a property value: -2 | -1 | 0 | 1 | 2.
int bandOf(double value);

// Same, but sticky: moving out of `currentBand` requires crossing the boundary by `hysteresis`,
// so a value hovering on a line does not flicker between two signs.
int bandOf(double value, int currentBand, double hysteresis);

std::vector<Sign> signsFor(const PropertyValues& values);
std::vector<Sign> signsFor(const PropertyValues& values, const std::array<int, kPropertyCount>& currentBands,
                           double hysteresis);

enum class EntryKind {
    Fed,
    Moved,         // habitat changed
    Assigned,      // job changed
    SignChanged,   // a visible sign appeared or changed band
    Stirring,      // close to a type it does not currently have
    Became,        // resolved to a new type
    Reverted,      // lost a type
    Locked,        // a type locked in permanently
    Note           // free text
};

struct Entry {
    double atHours;       // simulation time
    EntryKind kind;
    std::string text;
};

class Journal {
public:
    void add(double atHours, EntryKind kind, std::string text);
    const std::vector<Entry>& entries() const { return entries_; }
    void clear() { entries_.clear(); }

private:
    std::vector<Entry> entries_;
};

std::string_view name(EntryKind k);
std::optional<EntryKind> entryKindFromName(std::string_view s);

} // namespace owlight
