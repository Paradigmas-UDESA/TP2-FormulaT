#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "formulat/section.h"

namespace formulat {

// One entry of the layout. Chicanes need an odd degree >= 3, kerbs an even
// degree >= 2 (see generate()).
struct SectionSpec {
    SectionType type;
    int degree;
    std::size_t targets;
};

// Everything that defines a track; generate() turns it into sections.
struct TrackConfig {
    std::string name;
    unsigned laps;
    double eps;          // referee tolerance
    double penaltyPct;   // +% of a section's time when ε < r <= 2ε
    std::uint32_t seed;
    std::vector<SectionSpec> layout;
};

struct Track {
    TrackConfig config;
    std::vector<Section> sections;  // one per layout entry; a lap runs them all in order
};

// Deterministic: the same config gives the same track (with the same standard
// library -- the distributions are implementation-defined). Throws
// std::invalid_argument on a degree the section type can't use.
Track generate(const TrackConfig& config);

// The calibrated circuit.
TrackConfig defaultTrack();

}  // namespace formulat
