// Clase 4: threads. These tests only check what doesn't depend on the tower
// being synchronized (clase 5 adds that): each car's own data is touched by
// its thread alone, so the race must give the same per-car results as the
// sequential run, an exception inside a thread must retire the car
// instead of killing the program, and the cars wait for the start lights.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "formulat/car.h"
#include "formulat/race.h"
#include "formulat/timingtower.h"
#include "formulat/track.h"
#include "formulat/sequential.h"

using namespace formulat;
using namespace std::chrono_literals;

namespace {

Track twoLaps()
{
    TrackConfig cfg = defaultTrack();
    cfg.laps = 2;
    return generate(cfg);
}

// tracks/stress.track, built here (no track file needed): one
// degree-1 section per lap, so every car hits the tower every microsecond.
Track stress(unsigned laps)
{
    const TrackConfig cfg{"Stress", laps, 1e-12, 50.0, 2026, {{SectionType::Straight, 1, 1}}};
    return generate(cfg);
}

// x^3 - 2x + 2 on [-2, 2]: Newton from the midpoint cycles 0 -> 1 -> 0 and
// throws after its 500 iterations.
Track newtonCycle()
{
    const TrackConfig cfg{"Cycle", 1, 1e-10, 50.0, 0, {{SectionType::Chicane, 3, 1}}};
    return {cfg, {Section{SectionType::Chicane, Polynomial({2, -2, 0, 1}), -2.0, 2.0, {0.0}}}};
}

void runRace(std::vector<Car>& grid, const Track& track, TimingTower& tower)
{
    Race race(grid, track, tower, nullptr);
    race.start();
    race.join();
}

}  // namespace

// --- the tower, on one thread ---------------------------------------------------

TEST(TowerTest, StartsWithEveryCarRunning)
{
    const Track t = twoLaps();
    const auto grid = makeGrid(t, 50ms);
    TimingTower tower(grid);
    const auto s = tower.snapshot();
    ASSERT_EQ(s.size(), grid.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        EXPECT_EQ(s[i].team, grid[i].team());
        EXPECT_EQ(s[i].driver, grid[i].driver());
        EXPECT_EQ(s[i].completed, 0u);
        EXPECT_EQ(s[i].total, std::chrono::milliseconds(50 * i));  // its grid slot
        EXPECT_EQ(s[i].state, CarStatus::State::Running);
        EXPECT_EQ(s[i].position, 0u);
    }
    EXPECT_FALSE(tower.allDone());
    EXPECT_EQ(tower.sectionsRun(), 0u);
}

TEST(TowerTest, ReportFinishRetire)
{
    const Track t = twoLaps();
    const auto grid = makeGrid(t);
    TimingTower tower(grid);
    tower.report(3, 7, 42ms);
    tower.report(3, 8, 50ms);
    tower.report(0, 1, 5ms);
    EXPECT_EQ(tower.sectionsRun(), 3u);
    EXPECT_EQ(tower.snapshot()[3].completed, 8u);
    EXPECT_EQ(tower.snapshot()[3].total, 50ms);

    EXPECT_EQ(tower.finish(5), 1u);
    EXPECT_EQ(tower.finish(2), 2u);
    tower.retire(7);
    const auto s = tower.snapshot();
    EXPECT_EQ(s[5].state, CarStatus::State::Finished);
    EXPECT_EQ(s[5].position, 1u);
    EXPECT_EQ(s[2].position, 2u);
    EXPECT_EQ(s[7].state, CarStatus::State::Retired);
    EXPECT_EQ(s[7].position, 0u);

    EXPECT_FALSE(tower.allDone());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        if (i != 5 && i != 2 && i != 7)
            tower.finish(i);
    }
    EXPECT_TRUE(tower.allDone());
}

// --- the race -----------------------------------------------------------------

TEST(RaceTest, MatchesSequential)
{
    const Track t = twoLaps();
    auto sequential = makeGrid(t);
    runSequential(sequential, t);

    auto grid = makeGrid(t);
    TimingTower tower(grid);
    runRace(grid, t, tower);

    ASSERT_EQ(grid.size(), sequential.size());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        SCOPED_TRACE(i);
        const CarResult& mine = grid[i].result();
        const CarResult& theirs = sequential[i].result();
        EXPECT_TRUE(grid[i].done());
        EXPECT_EQ(grid[i].completed(), sequential[i].completed());
        EXPECT_EQ(mine.finished, theirs.finished);
        ASSERT_EQ(mine.sections.size(), theirs.sections.size());
        for (std::size_t j = 0; j < mine.sections.size(); ++j) {
            EXPECT_EQ(mine.sections[j].outcome, theirs.sections[j].outcome);
            EXPECT_EQ(mine.sections[j].pitStop, theirs.sections[j].pitStop);
        }
        ASSERT_EQ(grid[i].adr().has_value(), sequential[i].adr().has_value());
        if (grid[i].adr()) {
            EXPECT_EQ(grid[i].adr()->reason, sequential[i].adr()->reason);
        }
    }
}

TEST(RaceTest, ExceptionInAThreadRetiresTheCar)
{
    const Track t = newtonCycle();
    auto grid = makeGrid(t);
    TimingTower tower(grid);
    runRace(grid, t, tower);  // an escaping exception would std::terminate here

    const Car& newtonCold = grid[4];
    ASSERT_EQ(newtonCold.team(), Team::Newton);
    ASSERT_EQ(newtonCold.driver(), Driver::Cold);
    EXPECT_TRUE(newtonCold.done());
    EXPECT_FALSE(newtonCold.result().finished);
    ASSERT_TRUE(newtonCold.adr().has_value());
    EXPECT_NE(newtonCold.adr()->reason.find("no convergence"), std::string::npos)
        << newtonCold.adr()->reason;
    EXPECT_EQ(tower.snapshot()[4].state, CarStatus::State::Retired);
}

// The section that threw is reported too: race's "sections run" check counts
// it. One car, one thread: no lost updates even without locks.
TEST(RaceTest, ThrownSectionIsReported)
{
    const Track t = newtonCycle();
    auto all = makeGrid(t);
    std::vector<Car> grid;
    grid.push_back(std::move(all[4]));  // Newton Cold
    TimingTower tower(grid);
    runRace(grid, t, tower);

    ASSERT_TRUE(grid[0].adr().has_value());
    EXPECT_EQ(grid[0].completed(), 0u);
    EXPECT_EQ(tower.sectionsRun(), 1u);
}

TEST(RaceTest, TowerAgreesWithTheCars)
{
    const Track t = twoLaps();
    auto grid = makeGrid(t);
    TimingTower tower(grid);
    runRace(grid, t, tower);

    EXPECT_TRUE(tower.allDone());
    const auto s = tower.snapshot();
    for (std::size_t i = 0; i < grid.size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(s[i].completed, grid[i].completed());
        EXPECT_EQ(s[i].total, grid[i].result().total);
        if (grid[i].result().finished) {
            EXPECT_EQ(s[i].state, CarStatus::State::Finished);
            EXPECT_GE(s[i].position, 1u);
        } else {
            EXPECT_EQ(s[i].state, CarStatus::State::Retired);
            EXPECT_EQ(s[i].position, 0u);
        }
    }
}

TEST(RaceTest, DestructorJoins)
{
    const Track t = twoLaps();
    auto grid = makeGrid(t);
    TimingTower tower(grid);
    {
        Race race(grid, t, tower, nullptr);
        race.start();
    }  // no join(): the destructor must wait, or std::terminate
    for (const Car& car : grid)
        EXPECT_TRUE(car.done());
}

// --- start lights ----------------------------------------------------------------------

TEST(LightsTest, NothingMovesBeforeLightsOut)
{
    const Track t = stress(10);
    auto grid = makeGrid(t);
    TimingTower tower(grid);
    Race r(grid, t, tower, nullptr, 50ms);  // lights out 250 ms after start()
    r.start();                                      // doesn't wait for the lights
    std::this_thread::sleep_for(100ms);
    EXPECT_LT(r.elapsed(), 0ns);
    EXPECT_EQ(tower.sectionsRun(), 0u);
    r.join();
    EXPECT_GE(r.elapsed(), 0ns);
    EXPECT_TRUE(tower.allDone());
}
