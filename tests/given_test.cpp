// Tests for the code given to students: the domain model, the referee, the
// engines, the track generator, runSection, the qualifying runner and the text
// files (track, results, accident data recorders), and Car's given private
// state (it makes Car move-only). They pass from day one;
// students never need to touch them.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "formulat/polynomial.h"
#include "formulat/section.h"
#include "formulat/track.h"
#include "formulat/engines.h"
#include "formulat/car.h"
#include "formulat/judge.h"
#include "formulat/qualifying.h"
#include "formulat/runsection.h"
#include "formulat/serialize.h"

using namespace formulat;

namespace {

const Team kAllTeams[] = {Team::Bisection, Team::Toms748, Team::Newton,
                          Team::Halley,    Team::Secant,  Team::RegulaFalsi};
const Team kBracketing[] = {Team::Bisection, Team::Toms748, Team::RegulaFalsi};

// x^3 + x on [-1, 1]: monotone, one root per target in (-2, 2).
Section cubic()
{
    return {SectionType::Straight, Polynomial({0, 1, 0, 1}), -1.0, 1.0,
            {-1.5, -0.5, 0.0, 0.7, 1.8}};
}

// (x - 0.5)^2 = 0 on [-1, 3]: a double root, no sign change.
Section kerb()
{
    return {SectionType::Kerb, Polynomial({0.25, -1, 1}), -1.0, 3.0, {0.0}};
}

std::vector<double> run(Engine& e, const Section& s, bool warm)
{
    e.prepare(s);
    std::vector<double> roots;
    std::optional<double> hint;
    for (double c : s.targets) {
        roots.push_back(e.solve(c, hint));
        if (warm)
            hint = roots.back();
    }
    return roots;
}

}  // namespace

// --- Polynomial --------------------------------------------------------------

TEST(PolynomialTest, Evaluates)
{
    Polynomial p({1, -3, 0, 2});  // 1 - 3x + 2x^3
    EXPECT_DOUBLE_EQ(p(0), 1);
    EXPECT_DOUBLE_EQ(p(1), 0);
    EXPECT_DOUBLE_EQ(p(2), 11);
    EXPECT_DOUBLE_EQ(p(-0.5), 2.25);
}

TEST(PolynomialTest, Derivative)
{
    Polynomial d = Polynomial({1, -3, 0, 2}).derivative();
    EXPECT_EQ(d.coefficients(), (std::vector<double>{-3, 0, 6}));
}

TEST(PolynomialTest, DerivativeOfConstantIsZero)
{
    EXPECT_EQ(Polynomial({5}).derivative().coefficients(), (std::vector<double>{0}));
}

TEST(PolynomialTest, Degree)
{
    EXPECT_EQ(Polynomial({7}).degree(), 0u);
    EXPECT_EQ(Polynomial({1, -3, 0, 2}).degree(), 3u);
}

TEST(PolynomialTest, EmptyThrows)
{
    EXPECT_THROW(Polynomial({}), std::invalid_argument);
}

// --- judge -------------------------------------------------------------------

TEST(JudgeTest, CleanPenaltyDnf)
{
    Section s{SectionType::Straight, Polynomial({0, 1}), -1.0, 1.0, {0.5}};  // p(x) = x
    EXPECT_EQ(judge(s, {0.5}, 1e-6), Outcome::Clean);
    EXPECT_EQ(judge(s, {0.5 + 1.5e-6}, 1e-6), Outcome::Penalty);
    EXPECT_EQ(judge(s, {0.5 + 3e-6}, 1e-6), Outcome::DNF);
}

TEST(JudgeTest, WorstTargetDecides)
{
    Section s{SectionType::Straight, Polynomial({0, 1}), -1.0, 1.0, {0.1, 0.2, 0.3}};
    EXPECT_EQ(judge(s, {0.1, 0.2 + 1.5e-6, 0.3}, 1e-6), Outcome::Penalty);
    EXPECT_EQ(judge(s, {0.1, 0.2 + 1.5e-6, 0.3 + 1e-3}, 1e-6), Outcome::DNF);
}

TEST(JudgeTest, NonFiniteIsDnf)
{
    Section s{SectionType::Straight, Polynomial({0, 1}), -1.0, 1.0, {0.5}};
    EXPECT_EQ(judge(s, {std::nan("")}, 1e-6), Outcome::DNF);
    EXPECT_EQ(judge(s, {INFINITY}, 1e-6), Outcome::DNF);
}

TEST(JudgeTest, OutsideIntervalIsDnf)
{
    // x^2 = 1 has roots -1 and 1; only 1 is on [0, 2].
    Section s{SectionType::Straight, Polynomial({0, 0, 1}), 0.0, 2.0, {1.0}};
    EXPECT_EQ(judge(s, {1.0}, 1e-6), Outcome::Clean);
    EXPECT_EQ(judge(s, {-1.0}, 1e-6), Outcome::DNF);
}

TEST(JudgeTest, SizeMismatchThrows)
{
    EXPECT_THROW(judge(cubic(), {0.0}, 1e-6), std::invalid_argument);
}

// --- engines -----------------------------------------------------------------

TEST(EngineTest, AllTeamsSolveAMonotoneCubic)
{
    for (Team team : kAllTeams) {
        auto e = makeEngine(team);
        SCOPED_TRACE(e->name());
        ASSERT_TRUE(e->canHandle(cubic()));
        EXPECT_EQ(judge(cubic(), run(*e, cubic(), false), 1e-10), Outcome::Clean) << "cold";
        EXPECT_EQ(judge(cubic(), run(*e, cubic(), true), 1e-10), Outcome::Clean) << "warm";
    }
}

TEST(EngineTest, BracketingTeamsPitOnAKerb)
{
    for (Team team : kBracketing) {
        auto e = makeEngine(team);
        SCOPED_TRACE(e->name());
        EXPECT_FALSE(e->canHandle(kerb()));
    }
}

TEST(EngineTest, FallbackSolvesAKerb)
{
    auto e = makeFallback();
    ASSERT_TRUE(e->canHandle(kerb()));
    EXPECT_EQ(judge(kerb(), run(*e, kerb(), false), 1e-10), Outcome::Clean);
}

TEST(EngineTest, NewtonCyclesAndGivesUp)
{
    // x^3 - 2x + 2 from x = 0: Newton jumps 0 -> 1 -> 0 -> ... forever.
    Section s{SectionType::Chicane, Polynomial({2, -2, 0, 1}), -2.0, 2.0, {0.0}};
    auto e = makeEngine(Team::Newton);
    e->prepare(s);
    EXPECT_THROW(e->solve(0.0, 0.0), std::runtime_error);
}

TEST(EngineTest, WarmBracketFindsTheRootNearTheHint)
{
    // (x - 0.1)(x - 0.5)(x - 0.9) = 0 on [0, 1]: three roots; cold bisection
    // would find the middle one, a warm start near 0.9 must find 0.9.
    Section s{SectionType::Sweepers, Polynomial({-0.045, 0.59, -1.5, 1}), 0.0, 1.0, {0.0}};
    for (Team team : kBracketing) {
        auto e = makeEngine(team);
        SCOPED_TRACE(e->name());
        e->prepare(s);
        EXPECT_NEAR(e->solve(0.0, 0.88), 0.9, 1e-9);
    }
}

// --- track generator -----------------------------------------------------------

TEST(TrackTest, SameSeedSameTrack)
{
    Track t1 = generate(defaultTrack());
    Track t2 = generate(defaultTrack());
    ASSERT_EQ(t1.sections.size(), t2.sections.size());
    for (std::size_t i = 0; i < t1.sections.size(); ++i) {
        EXPECT_EQ(t1.sections[i].p.coefficients(), t2.sections[i].p.coefficients());
        EXPECT_EQ(t1.sections[i].targets, t2.sections[i].targets);
    }
}

TEST(TrackTest, DifferentSeedDifferentTrack)
{
    TrackConfig other = defaultTrack();
    ++other.seed;
    EXPECT_NE(generate(defaultTrack()).sections[0].targets, generate(other).sections[0].targets);
}

TEST(TrackTest, OneSectionPerLayoutEntry)
{
    Track t = generate(defaultTrack());
    ASSERT_EQ(t.sections.size(), t.config.layout.size());
    for (std::size_t i = 0; i < t.sections.size(); ++i) {
        EXPECT_EQ(t.sections[i].type, t.config.layout[i].type);
        EXPECT_EQ(t.sections[i].p.degree(), static_cast<std::size_t>(t.config.layout[i].degree));
        EXPECT_EQ(t.sections[i].targets.size(), t.config.layout[i].targets);
    }
}

// What every construction promises, checked on a grid.
TEST(TrackTest, SectionsKeepTheirPromises)
{
    for (const Section& s : generate(defaultTrack()).sections) {
        SCOPED_TRACE(static_cast<int>(s.type));
        const int n = 2000;
        double lo = INFINITY, hi = -INFINITY, prev = -INFINITY;
        const bool increasing = s.type != SectionType::Kerb;
        for (int i = 0; i <= n; ++i) {
            const double y = s.p(s.a + (s.b - s.a) * i / n);
            EXPECT_LE(std::abs(y), 1 + 1e-12);
            if (increasing) {
                EXPECT_GE(y, prev);
            }
            lo = std::min(lo, y);
            hi = std::max(hi, y);
            prev = y;
        }
        for (double c : s.targets) {  // every target has a root in [a, b]
            EXPECT_GE(c, lo);
            EXPECT_LE(c, hi);
        }
        if (s.type == SectionType::Sweepers) {
            EXPECT_TRUE(std::is_sorted(s.targets.begin(), s.targets.end()));
        }
        if (s.type == SectionType::Chicane) {
            EXPECT_EQ(s.p.derivative()(0.0), 0.0);  // flat at the cold start
            for (double c : s.targets)
                EXPECT_GT(c, s.p(0.3));  // roots right of the flat stretch
        }
        if (s.type == SectionType::Kerb) {
            const double p0 = s.p(s.a);
            EXPECT_TRUE(std::any_of(s.targets.begin(), s.targets.end(),
                                    [&](double c) { return c > p0; }));  // crossed twice
        }
    }
}

TEST(TrackTest, BadDegreeThrows)
{
    auto withOne = [](SectionType type, int degree) {
        TrackConfig c = defaultTrack();
        c.layout = {{type, degree, 10}};
        return c;
    };
    EXPECT_THROW(generate(withOne(SectionType::Straight, 0)), std::invalid_argument);
    EXPECT_THROW(generate(withOne(SectionType::Chicane, 6)), std::invalid_argument);
    EXPECT_THROW(generate(withOne(SectionType::Chicane, 1)), std::invalid_argument);
    EXPECT_THROW(generate(withOne(SectionType::Kerb, 5)), std::invalid_argument);
}

TEST(TrackTest, BracketingTeamsPitOnlyOnKerbs)
{
    for (const Section& s : generate(defaultTrack()).sections)
        for (Team team : kBracketing)
            EXPECT_EQ(makeEngine(team)->canHandle(s), s.type != SectionType::Kerb);
}

// --- runSection ----------------------------------------------------------------

namespace {

// Answers x = c + offset (so on p(x) = x the residual is |offset|), or throws.
class FixedEngine : public Engine {
public:
    FixedEngine(double offset, bool handles = true, bool throws = false)
        : offset_(offset), handles_(handles), throws_(throws) {}
    std::string name() const override { return "Fixed"; }
    bool canHandle(const Section&) const override { return handles_; }
    void prepare(const Section&) override {}
    double solve(double c, std::optional<double> hint) override
    {
        if (throws_)
            throw std::runtime_error("boom");
        hints.push_back(hint);
        return c + offset_;
    }
    std::vector<std::optional<double>> hints;

private:
    double offset_;
    bool handles_;
    bool throws_;
};

Section identity()
{
    return {SectionType::Straight, Polynomial({0, 1}), -1.0, 1.0, {0.1, 0.2, 0.3}};
}

}  // namespace

TEST(RunSectionTest, OutcomesFollowTheReferee)
{
    FixedEngine clean(0), penalty(1.5e-6), dnf(3e-6), fallback(0);
    EXPECT_EQ(runSection(clean, fallback, identity(), Driver::Cold, 1e-6, 50).outcome, Outcome::Clean);
    EXPECT_EQ(runSection(penalty, fallback, identity(), Driver::Cold, 1e-6, 50).outcome, Outcome::Penalty);
    EXPECT_EQ(runSection(dnf, fallback, identity(), Driver::Cold, 1e-6, 50).outcome, Outcome::DNF);
    EXPECT_NEAR(runSection(penalty, fallback, identity(), Driver::Cold, 1e-6, 50).residual, 1.5e-6, 1e-12);
}

TEST(RunSectionTest, PitsWhenTheEngineCantHandleIt)
{
    FixedEngine engine(1.0, /*handles=*/false), fallback(0);
    SectionResult r = runSection(engine, fallback, identity(), Driver::Cold, 1e-6, 50);
    EXPECT_TRUE(r.pitStop);
    EXPECT_EQ(r.outcome, Outcome::Clean);  // the fallback's answers were judged
    EXPECT_TRUE(engine.hints.empty());
    EXPECT_EQ(fallback.hints.size(), 3u);
}

TEST(RunSectionTest, DriversPassTheirHints)
{
    FixedEngine cold(0), warm(0), fallback(0);
    runSection(cold, fallback, identity(), Driver::Cold, 1e-6, 50);
    runSection(warm, fallback, identity(), Driver::Warm, 1e-6, 50);
    EXPECT_EQ(cold.hints, (std::vector<std::optional<double>>{std::nullopt, std::nullopt, std::nullopt}));
    // Warm: enters at b, then the previous root.
    EXPECT_EQ(warm.hints, (std::vector<std::optional<double>>{1.0, 0.1, 0.2}));
}

TEST(RunSectionTest, EngineExceptionsPropagate)
{
    FixedEngine engine(0, true, /*throws=*/true), fallback(0);
    EXPECT_THROW(runSection(engine, fallback, identity(), Driver::Cold, 1e-6, 50), std::runtime_error);
}

// The calibration contract (PLAN.md): over a lap of the default track, the
// only DNFs are the cold Newton and the cold secant, on the chicanes. Nobody
// else fails or throws. Deterministic, so a plain test.
TEST(RaceTest, OnlyTheColdOpenDriversCrash)
{
    const Track track = generate(defaultTrack());
    auto fallback = makeFallback();
    std::set<std::string> dnfs;
    for (Team team : kAllTeams) {
        for (Driver driver : {Driver::Cold, Driver::Warm}) {
            auto engine = makeEngine(team);
            const std::string car = engine->name() + (driver == Driver::Cold ? " cold" : " warm");
            for (const Section& s : track.sections) {
                SectionResult r{};
                ASSERT_NO_THROW(r = runSection(*engine, *fallback, s, driver, track.config.eps,
                                               track.config.penaltyPct))
                    << car;
                if (r.outcome == Outcome::DNF) {
                    EXPECT_EQ(s.type, SectionType::Chicane) << car;
                    dnfs.insert(car);
                    break;
                }
            }
        }
    }
    EXPECT_EQ(dnfs, (std::set<std::string>{"Newton cold", "Secant cold"}));
}

// --- qualifying ------------------------------------------------------------------

TEST(QualifyingTest, TwelveCarsAndAnAdrPerDnf)
{
    const Qualifying q = qualify(generate(defaultTrack()));
    ASSERT_EQ(q.cars.size(), 12u);
    ASSERT_EQ(q.adrs.size(), 2u);
    for (const AccidentDataRecorder& adr : q.adrs) {
        EXPECT_EQ(adr.driver, Driver::Cold);
        EXPECT_TRUE(adr.team == Team::Newton || adr.team == Team::Secant);
        EXPECT_EQ(adr.lap, 1u);
        EXPECT_EQ(adr.type, SectionType::Chicane);
        EXPECT_EQ(adr.type, generate(defaultTrack()).sections[adr.section].type);
    }
    for (const CarResult& car : q.cars) {
        EXPECT_EQ(car.finished, car.sections.size() == defaultTrack().layout.size() &&
                                    car.sections.back().outcome != Outcome::DNF);
        std::chrono::nanoseconds sum{0};
        for (const SectionResult& r : car.sections)
            sum += r.time;
        EXPECT_EQ(car.total, sum);
    }
}

TEST(QualifyingTest, ClassifyPutsFinishersFirstThenDnfsByProgress)
{
    using namespace std::chrono_literals;
    const SectionResult ok{Outcome::Clean, false, 1ms, 0.0};
    const SectionResult dnf{Outcome::DNF, false, 1ms, NAN};
    const std::vector<CarResult> cars{
        {Team::Newton, Driver::Cold, false, 1ms, {dnf}},            // DNF, 0 completed
        {Team::Halley, Driver::Cold, true, 9ms, {ok, ok}},           // 2nd
        {Team::Secant, Driver::Cold, false, 2ms, {ok, dnf}},         // DNF, 1 completed
        {Team::Bisection, Driver::Warm, true, 5ms, {ok, ok}},        // 1st
    };
    const auto grid = classify(cars);
    EXPECT_EQ(grid[0].team, Team::Bisection);
    EXPECT_EQ(grid[1].team, Team::Halley);
    EXPECT_EQ(grid[2].team, Team::Secant);
    EXPECT_EQ(grid[3].team, Team::Newton);
}

// --- text files ---------------------------------------------------------------------------

namespace {

void expectSameTrack(const TrackConfig& x, const TrackConfig& y)
{
    EXPECT_EQ(x.name, y.name);
    EXPECT_EQ(x.laps, y.laps);
    EXPECT_EQ(std::memcmp(&x.eps, &y.eps, sizeof x.eps), 0);
    EXPECT_EQ(std::memcmp(&x.penaltyPct, &y.penaltyPct, sizeof x.penaltyPct), 0);
    EXPECT_EQ(x.seed, y.seed);
    ASSERT_EQ(x.layout.size(), y.layout.size());
    for (std::size_t i = 0; i < x.layout.size(); ++i) {
        EXPECT_EQ(x.layout[i].type, y.layout[i].type);
        EXPECT_EQ(x.layout[i].degree, y.layout[i].degree);
        EXPECT_EQ(x.layout[i].targets, y.layout[i].targets);
    }
}

}  // namespace

TEST(TrackFileTest, RoundTrip)
{
    TrackConfig cfg = defaultTrack();
    cfg.name = "Autódromo \"Oscar\" Gálvez";
    const std::string path =
        (std::filesystem::temp_directory_path() / "formulat-test-track").string();
    saveTrackConfig(path, cfg);
    expectSameTrack(loadTrackConfig(path), cfg);
}

TEST(TrackFileTest, ShippedFileIsTheDefaultTrack)
{
    expectSameTrack(loadTrackConfig(std::string(FORMULAT_SOURCE_DIR) + "/tracks/formula-t.track"),
                    defaultTrack());
}

// --- Car's given state -------------------------------------------------------------------

// Car owns its engines through unique_ptr: no copies (two cars would share an
// engine, and engines keep state from prepare()), but moves (vector<Car>).
TEST(CarTest, IsMoveOnly)
{
    EXPECT_FALSE(std::is_copy_constructible_v<Car>) << "a Car owns its engines: no copies";
    EXPECT_TRUE(std::is_move_constructible_v<Car>) << "vector<Car> needs to move them";
}
