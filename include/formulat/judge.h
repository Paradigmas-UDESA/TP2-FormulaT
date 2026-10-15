#pragma once

#include <vector>

#include "formulat/section.h"

namespace formulat {

enum class Outcome { Clean, Penalty, DNF };

// The referee. roots[i] is the car's answer for s.targets[i]; the worst
// residual |p(x) - c| decides: <= eps Clean, <= 2 eps Penalty, else DNF.
// Also DNF: NaN/inf, or any root outside [a, b].
// Throws std::invalid_argument if roots and targets differ in size.
Outcome judge(const Section& s, const std::vector<double>& roots, double eps);

}  // namespace formulat
