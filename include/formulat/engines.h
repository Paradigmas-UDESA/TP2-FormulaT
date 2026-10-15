#pragma once

#include <memory>
#include <optional>
#include <string>

#include "formulat/section.h"

namespace formulat {

enum class Team { Bisection, Toms748, Newton, Halley, Secant, RegulaFalsi };

// A root-finding algorithm (Strategy). Per section: canHandle, then prepare
// once, then solve once per target.
class Engine {
public:
    virtual ~Engine() = default;

    virtual std::string name() const = 0;

    // False when this engine can't solve some target of s (bracketing engines:
    // no sign change of p - c over [a, b]). The car then pits.
    virtual bool canHandle(const Section& s) const = 0;

    // Per-section work: keeps p, a, b and whatever derivatives it needs.
    virtual void prepare(const Section& s) = 0;

    // Solves p(x) = c. hint: the previous target's root (warm start), or
    // nullopt (cold start). Throws std::runtime_error after 100 iterations.
    // Newton and secant are unguarded: they may return NaN/inf or an x
    // outside [a, b]; judging that is the referee's job.
    virtual double solve(double c, std::optional<double> hint) = 0;
};

std::unique_ptr<Engine> makeEngine(Team team);

// The pit crew: guarded Newton, works without a sign change as long as the
// root is in [a, b]. Not a team.
std::unique_ptr<Engine> makeFallback();

}  // namespace formulat
