#include "formulat/engines.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <boost/math/tools/roots.hpp>
#include <boost/math/tools/toms748_solve.hpp>

// Bisection, TOMS 748 and Halley wrap Boost.Math. Newton, secant and regula
// falsi are hand-written: Boost's Newton-type solvers are guarded, and these
// must be free to fail. The fallback is hand-written too (see below).

namespace formulat {

namespace {

// The stopping rule, the same for every engine: the step (or bracket width)
// is at most 2^-40 relative to max(1, |x|), within 500 iterations (regula
// falsi's one-sided stall on a chicane needs ~130).
constexpr int kBits = 40;
constexpr std::uintmax_t kMaxIter = 500;
const double kStepTol = std::ldexp(1.0, -kBits);

bool converged(double dx, double x)
{
    return std::abs(dx) <= kStepTol * std::max(1.0, std::abs(x));
}

// Boost's own eps_tolerance is purely relative and never stops on a root at 0.
bool bracketConverged(double lo, double hi)
{
    return converged(hi - lo, std::max(std::abs(lo), std::abs(hi)));
}

// Boost reports how many iterations it used; using them all means it gave up.
void checkIterations(std::uintmax_t used, const std::string& who)
{
    if (used >= kMaxIter)
        throw std::runtime_error(who + ": no convergence in " + std::to_string(kMaxIter) + " iterations");
}

[[noreturn]] void gaveUp(const std::string& who)
{
    checkIterations(kMaxIter, who);
    throw std::logic_error("unreachable");
}

class Base : public Engine {
public:
    void prepare(const Section& s) override
    {
        p_ = s.p;
        a_ = s.a;
        b_ = s.b;
    }

protected:
    double start(std::optional<double> hint) const
    {
        return hint ? *hint : (a_ + b_) / 2;
    }

    Polynomial p_{{0.0}};
    double a_ = 0.0;
    double b_ = 0.0;
};

// Engines that keep a sign change around the root.
class Bracketing : public Base {
public:
    bool canHandle(const Section& s) const override
    {
        const double pa = s.p(s.a);
        const double pb = s.p(s.b);
        for (double c : s.targets)
            if (!((pa - c) * (pb - c) <= 0))
                return false;
        return true;
    }

    double solve(double c, std::optional<double> hint) override
    {
        auto [lo, hi] = bracket(c, hint);
        return solveIn(c, lo, hi);
    }

protected:
    virtual double solveIn(double c, double lo, double hi) = 0;

private:
    // Cold: [a, b]. Warm: a small bracket around the hint, doubled until it
    // has a sign change. Ends at [a, b] at worst, which has one (canHandle).
    std::pair<double, double> bracket(double c, std::optional<double> hint) const
    {
        if (!hint)
            return {a_, b_};
        const double h = std::clamp(*hint, a_, b_);
        for (double w = (b_ - a_) / 1024;; w *= 2) {
            const double lo = std::max(a_, h - w);
            const double hi = std::min(b_, h + w);
            if ((p_(lo) - c) * (p_(hi) - c) <= 0 || (lo == a_ && hi == b_))
                return {lo, hi};
        }
    }
};

class Bisection : public Bracketing {
public:
    std::string name() const override { return "Bisection"; }

protected:
    double solveIn(double c, double lo, double hi) override
    {
        std::uintmax_t it = kMaxIter;
        auto r = boost::math::tools::bisect([&](double x) { return p_(x) - c; }, lo, hi,
                                            bracketConverged, it);
        checkIterations(it, name());
        return (r.first + r.second) / 2;
    }
};

class Toms748 : public Bracketing {
public:
    std::string name() const override { return "TOMS 748"; }

protected:
    double solveIn(double c, double lo, double hi) override
    {
        std::uintmax_t it = kMaxIter;
        auto r = boost::math::tools::toms748_solve([&](double x) { return p_(x) - c; }, lo, hi,
                                                   bracketConverged, it);
        checkIterations(it, name());
        return (r.first + r.second) / 2;
    }
};

class RegulaFalsi : public Bracketing {
public:
    std::string name() const override { return "Regula falsi"; }

protected:
    double solveIn(double c, double lo, double hi) override
    {
        double flo = p_(lo) - c;
        double fhi = p_(hi) - c;
        double prev = std::nan("");
        for (std::uintmax_t i = 0; i < kMaxIter; ++i) {
            const double x = hi - fhi * (hi - lo) / (fhi - flo);
            const double fx = p_(x) - c;
            if (fx == 0 || converged(x - prev, x))
                return x;
            if ((fx < 0) == (flo < 0)) {
                lo = x;
                flo = fx;
            } else {
                hi = x;
                fhi = fx;
            }
            prev = x;
        }
        gaveUp(name());
    }
};

// Open engines: no bracket, so they accept any section.
class Open : public Base {
public:
    bool canHandle(const Section&) const override { return true; }

    void prepare(const Section& s) override
    {
        Base::prepare(s);
        dp_ = p_.derivative();
        ddp_ = dp_.derivative();
    }

protected:
    Polynomial dp_{{0.0}};
    Polynomial ddp_{{0.0}};
};

// Unguarded: nothing stops it from dividing by p'(x) = 0 or leaving [a, b].
class Newton : public Open {
public:
    std::string name() const override { return "Newton"; }

    double solve(double c, std::optional<double> hint) override
    {
        double x = start(hint);
        for (std::uintmax_t i = 0; i < kMaxIter; ++i) {
            const double dx = (p_(x) - c) / dp_(x);
            x -= dx;
            if (!std::isfinite(x) || converged(dx, x))
                return x;
        }
        gaveUp(name());
    }
};

// Unguarded, like Newton; the derivative is replaced by a finite difference.
class Secant : public Open {
public:
    std::string name() const override { return "Secant"; }

    double solve(double c, std::optional<double> hint) override
    {
        double x0 = start(hint);
        double x1 = x0 + (b_ - a_) / 1024;
        double f0 = p_(x0) - c;
        double f1 = p_(x1) - c;
        for (std::uintmax_t i = 0; i < kMaxIter; ++i) {
            const double dx = f1 * (x1 - x0) / (f1 - f0);
            x0 = x1;
            f0 = f1;
            x1 -= dx;
            if (!std::isfinite(x1) || converged(dx, x1))
                return x1;
            f1 = p_(x1) - c;
        }
        gaveUp(name());
    }
};

// Boost's guarded Halley: stays in [a, b].
class Halley : public Open {
public:
    std::string name() const override { return "Halley"; }

    double solve(double c, std::optional<double> hint) override
    {
        std::uintmax_t it = kMaxIter;
        const double x = boost::math::tools::halley_iterate(
            [&](double x) { return std::make_tuple(p_(x) - c, dp_(x), ddp_(x)); },
            std::clamp(start(hint), a_, b_), a_, b_, kBits, it);
        checkIterations(it, name());
        return x;
    }
};

// Guarded Newton: a step that would leave [a, b] goes halfway to the bound
// instead. Boost's newton_raphson_iterate was the first choice, but its
// damping heuristics wander across a kerb's top to the wrong branch.
class Fallback : public Open {
public:
    std::string name() const override { return "Fallback"; }

    double solve(double c, std::optional<double> hint) override
    {
        double x = std::clamp(start(hint), a_, b_);
        for (std::uintmax_t i = 0; i < kMaxIter; ++i) {
            double next = x - (p_(x) - c) / dp_(x);
            if (!(next >= a_))
                next = (x + a_) / 2;
            else if (next > b_)
                next = (x + b_) / 2;
            const double dx = next - x;
            x = next;
            if (converged(dx, x))
                return x;
        }
        gaveUp(name());
    }
};

}  // namespace

std::unique_ptr<Engine> makeEngine(Team team)
{
    switch (team) {
    case Team::Bisection:   return std::make_unique<Bisection>();
    case Team::Toms748:     return std::make_unique<Toms748>();
    case Team::Newton:      return std::make_unique<Newton>();
    case Team::Halley:      return std::make_unique<Halley>();
    case Team::Secant:      return std::make_unique<Secant>();
    case Team::RegulaFalsi: return std::make_unique<RegulaFalsi>();
    }
    throw std::invalid_argument("makeEngine: unknown team");
}

std::unique_ptr<Engine> makeFallback()
{
    return std::make_unique<Fallback>();
}

}  // namespace formulat
