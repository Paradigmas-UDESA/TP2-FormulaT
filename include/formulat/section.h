#pragma once

#include <vector>

#include "formulat/polynomial.h"

namespace formulat {

// The family of problems a section belongs to; each favours different engines
// and drivers (see PLAN.md, "The track").
enum class SectionType { Straight, Hairpin, Sweepers, Chicane, Kerb };

// Solve p(x) = c for every c in targets, with x in [a, b].
struct Section {
    SectionType type;
    Polynomial p;
    double a;
    double b;
    std::vector<double> targets;
};

}  // namespace formulat
