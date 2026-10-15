#include "formulat/judge.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace formulat {

Outcome judge(const Section& s, const std::vector<double>& roots, double eps)
{
    if (roots.size() != s.targets.size())
        throw std::invalid_argument("judge: one root per target");

    double worst = 0.0;
    for (std::size_t i = 0; i < roots.size(); ++i) {
        const double x = roots[i];
        // Written so NaN fails: every comparison with NaN is false.
        if (!(x >= s.a && x <= s.b))
            return Outcome::DNF;
        const double r = std::abs(s.p(x) - s.targets[i]);
        if (!(r <= 2 * eps))
            return Outcome::DNF;
        worst = std::max(worst, r);
    }
    return worst <= eps ? Outcome::Clean : Outcome::Penalty;
}

}  // namespace formulat
