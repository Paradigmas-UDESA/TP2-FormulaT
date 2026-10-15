// Clase 3: containers. Each car's route is a queue of section indices; the
// grid is a vector of cars.

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <queue>
#include <stdexcept>
#include <vector>

#include "formulat/car.h"
#include "formulat/track.h"
#include "formulat/qualifying.h"
#include "formulat/sequential.h"

using namespace formulat;

namespace {

// The default track, cut to `laps` laps of its first `sections` sections.
Track smallTrack(unsigned laps, std::size_t sections)
{
    TrackConfig cfg = defaultTrack();
    cfg.laps = laps;
    cfg.layout.resize(sections);
    return generate(cfg);
}

std::vector<std::size_t> drain(std::queue<std::size_t> q)
{
    std::vector<std::size_t> v;
    for (; !q.empty(); q.pop())
        v.push_back(q.front());
    return v;
}

// x^3 - 2x + 2 on [-2, 2], one section per lap: Newton from the midpoint
// cycles 0 -> 1 -> 0 and throws after its 500 iterations.
Track newtonCycle(unsigned laps)
{
    const TrackConfig cfg{"Cycle", laps, 1e-10, 50.0, 0, {{SectionType::Chicane, 3, 1}}};
    return {cfg, {Section{SectionType::Chicane, Polynomial({2, -2, 0, 1}), -2.0, 2.0, {0.0}}}};
}

bool sameBits(double x, double y)
{
    return std::memcmp(&x, &y, sizeof x) == 0;
}

}  // namespace

// --- makeQueue -------------------------------------------------------------------

TEST(QueueTest, LapsTimesSectionsInOrder)
{
    EXPECT_EQ(drain(makeQueue(smallTrack(2, 3))), (std::vector<std::size_t>{0, 1, 2, 0, 1, 2}));
}

TEST(QueueTest, SkipsWhatIsCompleted)
{
    const Track t = smallTrack(2, 3);
    EXPECT_EQ(drain(makeQueue(t, 4)), (std::vector<std::size_t>{1, 2}));
    EXPECT_TRUE(makeQueue(t, 6).empty());
    EXPECT_THROW(makeQueue(t, 7), std::invalid_argument);
}

// --- makeGrid --------------------------------------------------------------------

TEST(GridTest, TwelveCarsInTeamThenDriverOrder)
{
    const auto grid = makeGrid(smallTrack(1, 3));
    const Team teams[] = {Team::Bisection, Team::Toms748, Team::Newton,
                          Team::Halley,    Team::Secant,  Team::RegulaFalsi};
    ASSERT_EQ(grid.size(), 12u);
    for (std::size_t i = 0; i < grid.size(); ++i) {
        EXPECT_EQ(grid[i].team(), teams[i / 2]);
        EXPECT_EQ(grid[i].driver(), i % 2 == 0 ? Driver::Cold : Driver::Warm);
        EXPECT_FALSE(grid[i].done());
        EXPECT_EQ(grid[i].completed(), 0u);
    }
}

// --- Car -------------------------------------------------------------------------

TEST(CarTest, RunsItsQueueToTheEnd)
{
    const Track t = smallTrack(2, 3);
    Car car(Team::Halley, Driver::Warm, t);
    for (std::size_t i = 1; i <= 6; ++i) {
        ASSERT_FALSE(car.done());
        car.runNext(t);
        EXPECT_EQ(car.completed(), i);
        EXPECT_EQ(car.result().sections.size(), i);
    }
    EXPECT_TRUE(car.done());
    EXPECT_TRUE(car.result().finished);
    EXPECT_FALSE(car.adr().has_value());
    car.runNext(t);  // done: nothing happens
    EXPECT_EQ(car.completed(), 6u);
}

TEST(CarTest, ResumesAfterCompleted)
{
    const Track t = smallTrack(2, 3);
    Car car(Team::Halley, Driver::Cold, t, 5);
    EXPECT_EQ(car.completed(), 5u);
    car.runNext(t);  // section 5 % 3 = 2 of lap 2
    EXPECT_EQ(car.completed(), 6u);
    EXPECT_TRUE(car.done());
    ASSERT_EQ(car.result().sections.size(), 1u);
}

TEST(CarTest, RefereeDnfRetiresWithAnAdr)
{
    const Track t = smallTrack(2, 5);  // section 3 is the first chicane
    Car car(Team::Newton, Driver::Cold, t);
    while (!car.done())
        car.runNext(t);
    EXPECT_FALSE(car.result().finished);
    EXPECT_EQ(car.completed(), 3u);
    ASSERT_TRUE(car.adr().has_value());
    const AccidentDataRecorder& adr = *car.adr();
    EXPECT_EQ(adr.team, Team::Newton);
    EXPECT_EQ(adr.driver, Driver::Cold);
    EXPECT_EQ(adr.lap, 1u);
    EXPECT_EQ(adr.section, 3u);
    EXPECT_EQ(adr.type, SectionType::Chicane);
    EXPECT_EQ(adr.time, car.result().total);
    EXPECT_FALSE(std::isfinite(adr.residual));
    EXPECT_EQ(adr.reason, "referee: a root was not a finite number");
    car.runNext(t);  // retired: nothing happens
    EXPECT_EQ(car.result().sections.size(), 4u);
}

// An engine exception escapes runNext; whoever runs the car retires it, and
// the ADR records the section that threw. Here the car resumes on lap 3.
TEST(CarTest, RetireFromOutside)
{
    const Track t = newtonCycle(3);
    Car car(Team::Newton, Driver::Cold, t, 2);
    EXPECT_THROW(car.runNext(t), std::runtime_error);
    car.retire("boom");
    EXPECT_TRUE(car.done());
    EXPECT_FALSE(car.result().finished);
    EXPECT_EQ(car.completed(), 2u);
    ASSERT_TRUE(car.adr().has_value());
    EXPECT_EQ(car.adr()->reason, "boom");
    EXPECT_TRUE(std::isnan(car.adr()->residual));
    EXPECT_EQ(car.adr()->lap, 3u);
    EXPECT_EQ(car.adr()->section, 0u);
    EXPECT_EQ(car.adr()->type, SectionType::Chicane);
    car.retire("again");  // retired once is enough
    EXPECT_EQ(car.adr()->reason, "boom");
}

// --- qualifying on the new structures ------------------------------------------------

// One lap of runSequential is qualifying: same cars, same outcomes, same pit
// stops, same DNFs and ADRs. Times differ from run to run, so they aren't compared.
TEST(SequentialTest, OneLapIsQualifying)
{
    TrackConfig cfg = defaultTrack();
    cfg.laps = 1;
    const Track t = generate(cfg);

    auto grid = makeGrid(t);
    runSequential(grid, t);
    const Qualifying q = qualify(t);

    ASSERT_EQ(grid.size(), q.cars.size());
    std::vector<AccidentDataRecorder> adrs;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const CarResult& mine = grid[i].result();
        const CarResult& theirs = q.cars[i];
        SCOPED_TRACE(i);
        EXPECT_EQ(mine.team, theirs.team);
        EXPECT_EQ(mine.driver, theirs.driver);
        EXPECT_EQ(mine.finished, theirs.finished);
        ASSERT_EQ(mine.sections.size(), theirs.sections.size());
        for (std::size_t j = 0; j < mine.sections.size(); ++j) {
            EXPECT_EQ(mine.sections[j].outcome, theirs.sections[j].outcome);
            EXPECT_EQ(mine.sections[j].pitStop, theirs.sections[j].pitStop);
        }
        if (grid[i].adr())
            adrs.push_back(*grid[i].adr());
    }
    ASSERT_EQ(adrs.size(), q.adrs.size());
    for (std::size_t i = 0; i < adrs.size(); ++i) {
        EXPECT_EQ(adrs[i].team, q.adrs[i].team);
        EXPECT_EQ(adrs[i].driver, q.adrs[i].driver);
        EXPECT_EQ(adrs[i].lap, q.adrs[i].lap);
        EXPECT_EQ(adrs[i].section, q.adrs[i].section);
        EXPECT_EQ(adrs[i].type, q.adrs[i].type);
        EXPECT_EQ(adrs[i].pitStops, q.adrs[i].pitStops);
        EXPECT_EQ(adrs[i].penalties, q.adrs[i].penalties);
        EXPECT_TRUE(sameBits(adrs[i].residual, q.adrs[i].residual));
        EXPECT_EQ(adrs[i].reason, q.adrs[i].reason);
    }
}
