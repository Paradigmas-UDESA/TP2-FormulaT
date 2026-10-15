#pragma once

#include <chrono>
#include <cstddef>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "formulat/track.h"
#include "formulat/engines.h"
#include "formulat/runsection.h"

namespace formulat {

// One car's session: every section it ran, in order.
struct CarResult {
    Team team;
    Driver driver;
    bool finished;
    std::chrono::nanoseconds total;        // sum of the sections' times (penalties included)
    std::vector<SectionResult> sections;   // ends at the DNF section, if the referee gave one

    // Binary, in this order: team, driver, finished, total, then sections as its
    // size (a size_t) followed by every SectionResult, each copied whole. Takes
    // any stream: a file (std::ofstream / std::ifstream) or a std::stringstream.
    // deserialize throws std::runtime_error if the input ends early. (binary.cpp)
    void serialize(std::ostream& out) const;
    void deserialize(std::istream& in);
};

// What a car leaves behind when it retires (the FIA's crash recorder).
struct AccidentDataRecorder {
    Team team;
    Driver driver;
    unsigned lap;                  // 1-based
    std::size_t section;           // index in the track, 0-based
    SectionType type;
    std::chrono::nanoseconds time; // accumulated before the crash
    unsigned pitStops;
    unsigned penalties;
    double residual;               // worst |p(x) - c| of the fatal section; NaN if it threw
    std::string reason;            // the referee's verdict, or the exception's what()
};

struct Qualifying {
    std::vector<CarResult> cars;               // in running order
    std::vector<AccidentDataRecorder> adrs;    // one per DNF
};

// Every car (6 teams x 2 drivers), one lap each, one after another.
Qualifying qualify(const Track& track);

// Finished cars by total time, then DNFs by sections completed (more is better).
std::vector<CarResult> classify(std::vector<CarResult> cars);

}  // namespace formulat
