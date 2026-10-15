#include "formulat/runsection.h"

#include <cmath>
#include <optional>
#include <vector>

namespace formulat {

// binary.h copies SectionResult as raw bytes: it must have no padding. A new
// field that breaks this breaks the build here, not the files.
static_assert(sizeof(SectionResult) == 24, "SectionResult has padding");

SectionResult runSection(Engine& engine, Engine& fallback, const Section& s, Driver driver,
                         double eps, double penaltyPct)
{
    using Clock = std::chrono::steady_clock;

    const bool pit = !engine.canHandle(s);
    Engine& e = pit ? fallback : engine;

    std::vector<double> roots;
    roots.reserve(s.targets.size());
    const auto start = Clock::now();
    e.prepare(s);
    // The warm driver enters each section from its end, then carries on
    // from each root; the cold driver lets the engine start from scratch.
    std::optional<double> hint;
    if (driver == Driver::Warm)
        hint = s.b;
    for (double c : s.targets) {
        roots.push_back(e.solve(c, hint));
        if (driver == Driver::Warm)
            hint = roots.back();
    }
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);

    const Outcome outcome = judge(s, roots, eps);
    if (outcome == Outcome::Penalty)
        time += std::chrono::duration_cast<std::chrono::nanoseconds>(time * (penaltyPct / 100));

    double worst = 0.0;
    for (std::size_t i = 0; i < roots.size(); ++i) {
        const double r = std::abs(s.p(roots[i]) - s.targets[i]);
        if (!(r <= worst))  // NaN sticks
            worst = r;
    }
    return {outcome, pit, time, worst};
}

std::string refereeVerdict(const SectionResult& r, double eps)
{
    if (!std::isfinite(r.residual))
        return "referee: a root was not a finite number";
    if (r.residual > 2 * eps)
        return "referee: residual above 2 eps";
    return "referee: a root outside [a, b]";
}

}  // namespace formulat
