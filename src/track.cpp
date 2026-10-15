#include "formulat/track.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

// How each section type is built (PLAN.md, "The track"). Every polynomial
// satisfies |p| <= 1 on [a, b] and has small coefficients (sum|coef| <= 3,
// kerbs a few hundred), so evaluating it in double costs far less than any ε.

namespace formulat {

namespace {

using Rng = std::mt19937;

double uniform(Rng& rng, double lo, double hi)
{
    return std::uniform_real_distribution<double>(lo, hi)(rng);
}

// Roots of chicane targets stay right of this, off the flat stretch at 0.
constexpr double kChicaneMargin = 0.3;
// Kerb targets above p(0) (crossed twice), per 100, and how high they go.
constexpr std::size_t kKerbTwicePer100 = 10;
constexpr double kKerbTop = 0.99;

// sum c_j x^j over the given powers, c_j ~ U(0,1)/j!, scaled so p(1) = 1.
// Positive coefficients: increasing and convex on [0, inf), so Newton from any
// start right of the flat part converges (overshooting to the right at most
// once). The 1/j! decay keeps high degrees benign: a hairpin costs its degree
// in evaluations, not in trouble.
std::vector<double> positiveSeries(Rng& rng, int degree, int first, int stride)
{
    std::vector<double> c(degree + 1, 0.0);
    double factorial = 1.0;
    double sum = 0.0;
    for (int j = 1; j <= degree; ++j) {
        factorial *= j;
        if (j >= first && (j - first) % stride == 0) {
            c[j] = uniform(rng, 0.0, 1.0) / factorial;
            sum += c[j];
        }
    }
    for (double& x : c)
        x /= sum;
    return c;
}

// Straight, hairpin, sweepers: on [0, 1], increasing from -1 to 1.
Section monotone(SectionType type, Rng& rng, const SectionSpec& spec)
{
    if (spec.degree < 1)
        throw std::invalid_argument("generate: degree must be >= 1");
    std::vector<double> c = positiveSeries(rng, spec.degree, 1, 1);
    for (double& x : c)
        x *= 2;
    c[0] = -1;
    Section s{type, Polynomial(std::move(c)), 0.0, 1.0, {}};
    for (std::size_t i = 0; i < spec.targets; ++i)
        s.targets.push_back(uniform(rng, -0.95, 0.95));
    if (type == SectionType::Sweepers)
        std::sort(s.targets.begin(), s.targets.end());
    return s;
}

// On [-1, 1]: odd powers from 3 up, so increasing, from -1 to 1, with p'(0) = 0
// exactly and a flat stretch around the midpoint. A cold Newton starts there
// and divides by zero; a cold secant's first step is enormous. Every root is
// > kChicaneMargin, so warm drivers (starting at the previous root) never see
// the flat part.
Section chicane(Rng& rng, const SectionSpec& spec)
{
    if (spec.degree < 3 || spec.degree % 2 == 0)
        throw std::invalid_argument("generate: a chicane needs an odd degree >= 3");
    Section s{SectionType::Chicane, Polynomial(positiveSeries(rng, spec.degree, 3, 2)), -1.0, 1.0, {}};
    const double lo = s.p(kChicaneMargin);
    for (std::size_t i = 0; i < spec.targets; ++i)
        s.targets.push_back(uniform(rng, lo, 0.95));
    return s;
}

// On [0, 1]: a hump, p = 1 - 2((x - e)/w)^degree with the top at e in
// (0.35, 0.42) and w = 1 - e, so p rises from p(0) to 1 and falls to -1 at
// x = 1. Targets above p(0) are crossed twice, so p - c has the same sign at
// both ends: the bracketing engines pit. Every target has a root on [e, 1],
// and so do the starts (0.5 cold, 1 for the first warm target): Newton on
// this concave branch never crosses e.
Section kerb(Rng& rng, const SectionSpec& spec)
{
    if (spec.degree < 2 || spec.degree % 2 != 0)
        throw std::invalid_argument("generate: a kerb needs an even degree >= 2");
    const double e = uniform(rng, 0.35, 0.42);
    const double w = 1 - e;
    // (x - e)^n by the binomial theorem.
    const int n = spec.degree;
    std::vector<double> c(n + 1);
    double binom = 1.0;
    for (int k = 0; k <= n; ++k) {
        c[k] = -2 * binom * std::pow(-e, n - k) / std::pow(w, n);
        binom = binom * (n - k) / (k + 1);
    }
    c[0] += 1;
    Section s{SectionType::Kerb, Polynomial(std::move(c)), 0.0, 1.0, {}};
    const double p0 = s.p(0.0);
    if (p0 > kKerbTop - 0.05)
        throw std::invalid_argument("generate: kerb too flat, lower its degree");
    const std::size_t twice = std::max<std::size_t>(1, spec.targets * kKerbTwicePer100 / 100);
    for (std::size_t i = 0; i < spec.targets; ++i)
        s.targets.push_back(i < twice ? uniform(rng, p0, kKerbTop) : uniform(rng, -0.95, 0.95));
    std::shuffle(s.targets.begin(), s.targets.end(), rng);
    return s;
}

}  // namespace

Track generate(const TrackConfig& config)
{
    Rng rng(config.seed);
    Track track{config, {}};
    for (const SectionSpec& spec : config.layout) {
        switch (spec.type) {
        case SectionType::Straight:
        case SectionType::Hairpin:
        case SectionType::Sweepers:
            track.sections.push_back(monotone(spec.type, rng, spec));
            break;
        case SectionType::Chicane:
            track.sections.push_back(chicane(rng, spec));
            break;
        case SectionType::Kerb:
            track.sections.push_back(kerb(rng, spec));
            break;
        }
    }
    return track;
}

TrackConfig defaultTrack()
{
    using T = SectionType;
    return {"Formula T", 1000, 1e-12, 50.0, 2026,
            {
                {T::Straight, 5, 1000},
                {T::Hairpin, 80, 1000},
                {T::Sweepers, 20, 1000},
                {T::Chicane, 7, 1000},
                {T::Straight, 5, 1000},
                {T::Kerb, 4, 1000},
                {T::Hairpin, 80, 1000},
                {T::Sweepers, 20, 1000},
                {T::Chicane, 7, 1000},
                {T::Kerb, 4, 1000},
            }};
}

}  // namespace formulat
