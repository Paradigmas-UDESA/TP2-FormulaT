#pragma once

#include <chrono>
#include <string>

#include "formulat/section.h"
#include "formulat/engines.h"
#include "formulat/judge.h"

namespace formulat {

// The two drivers of a team: same engine, different starting point.
enum class Driver {
    Cold,  // every target from scratch
    Warm,  // first target from b, then from the previous target's root
};

struct SectionResult {
    Outcome outcome;
    // 0 or 1: the fallback solved it. An int, not a bool: every field is 4 or 8
    // bytes, so the struct can be copied whole (binary.h).
    int pitStop;
    std::chrono::nanoseconds time;   // prepare + every solve, plus the penalty
    double residual;                 // worst |p(x) - c| (NaN if some x was)
};

// One driver over one section: canHandle (else pit: the fallback solves it),
// prepare, solve every target, then the referee. Engine exceptions propagate:
// catching them is the car's job.
SectionResult runSection(Engine& engine, Engine& fallback, const Section& s, Driver driver,
                         double eps, double penaltyPct);

// Why the referee gave r a DNF, for the accident data recorder.
std::string refereeVerdict(const SectionResult& r, double eps);

}  // namespace formulat
