// Clase 5: mutexes. Races are non-deterministic, so these tests check
// invariants, many times over, instead of exact results: with the tower
// unlocked they fail now and then (run them a few times), and the TSan build
// must report nothing (make test-clase5 runs them under it too). Then the red
// flag: stop, save the order, restart from it.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "formulat/car.h"
#include "formulat/race.h"
#include "formulat/timingtower.h"
#include "formulat/track.h"
#include "formulat/redflag.h"
#include "formulat/sequential.h"

using namespace formulat;
using namespace std::chrono_literals;

namespace {

#ifdef __SANITIZE_THREAD__
constexpr int kRaces = 3;  // TSan runs everything ~10x slower
#else
constexpr int kRaces = 20;
#endif

// tracks/stress.track, built here (no track file needed): one
// degree-1 section per lap, so every car hits the tower every microsecond.
Track stress(unsigned laps)
{
    const TrackConfig cfg{"Stress", laps, 1e-12, 50.0, 2026, {{SectionType::Straight, 1, 1}}};
    return generate(cfg);
}

Track shortRace(unsigned laps)
{
    TrackConfig cfg = defaultTrack();
    cfg.laps = laps;
    return generate(cfg);
}

// Every section a car ran was reported once, including one that threw.
std::size_t sectionsRunByTheCars(const std::vector<Car>& grid)
{
    std::size_t n = 0;
    for (const Car& car : grid) {
        const bool threw = car.adr() && car.result().sections.size() == car.completed();
        n += car.result().sections.size() + (threw ? 1 : 0);
    }
    return n;
}

// Waits (at most 30 s) until some car in the tower satisfies pred.
template <typename Pred>
bool waitFor(const TimingTower& tower, Pred pred)
{
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto s = tower.snapshot();
        if (std::any_of(s.begin(), s.end(), pred))
            return true;
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

std::string tempPath(const std::string& name)
{
    return (std::filesystem::temp_directory_path() / ("formulat-test-" + name)).string();
}

}  // namespace

// --- the tower, on one thread ------------------------------------------------------

TEST(TowerTest, StoppedCountsAsDone)
{
    const Track t = shortRace(1);
    const auto grid = makeGrid(t);
    TimingTower tower(grid);
    tower.stop(3);
    EXPECT_EQ(tower.snapshot()[3].state, CarStatus::State::Stopped);
    EXPECT_EQ(tower.snapshot()[3].position, 0u);
    for (std::size_t i = 0; i < grid.size(); ++i) {
        if (i != 3)
            tower.finish(i);
    }
    EXPECT_TRUE(tower.allDone());
}

// --- invariants, race after race ----------------------------------------------------

TEST(InvariantTest, NothingLostAndPositionsArePermutation)
{
    const Track t = stress(10000);
    for (int race = 0; race < kRaces; ++race) {
        SCOPED_TRACE(race);
        auto grid = makeGrid(t);
        TimingTower tower(grid);
        Race r(grid, t, tower, nullptr);
        r.start();

        // Meanwhile, the main thread reads: no car ever goes backwards.
        std::vector<std::size_t> seen(grid.size(), 0);
        while (!tower.allDone()) {
            const auto s = tower.snapshot();
            for (std::size_t i = 0; i < s.size(); ++i) {
                EXPECT_GE(s[i].completed, seen[i]);
                seen[i] = s[i].completed;
            }
        }
        r.join();

        EXPECT_EQ(tower.sectionsRun(), sectionsRunByTheCars(grid));
        const auto s = tower.snapshot();
        std::vector<unsigned> positions;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (grid[i].result().finished) {
                EXPECT_EQ(s[i].state, CarStatus::State::Finished);
                positions.push_back(s[i].position);
            } else {
                EXPECT_EQ(s[i].state, CarStatus::State::Retired);
            }
        }
        std::sort(positions.begin(), positions.end());
        for (std::size_t p = 0; p < positions.size(); ++p)
            EXPECT_EQ(positions[p], p + 1);
    }
}

// The cars share the log. Its format isn't checked: the point is that the
// TSan build sees every car write to the same stream.
TEST(LogTest, EveryCarWritesToTheSameLog)
{
    const Track t = stress(1000);
    auto grid = makeGrid(t);
    TimingTower tower(grid);
    std::ostringstream log;
    {
        Race r(grid, t, tower, &log);
        r.start();
    }
    std::istringstream lines(log.str());
    std::size_t n = 0;
    for (std::string line; std::getline(lines, line);)
        ++n;
    EXPECT_GE(n, grid.size());  // at least the finish (or retire) of each car
}

// --- the red flag ----------------------------------------------------------------------

TEST(RedFlagTest, EveryCarStopsAtTheEndOfALap)
{
    const Track t = shortRace(100);
    const std::size_t perLap = t.sections.size();
    auto grid = makeGrid(t);
    TimingTower tower(grid);
    Race r(grid, t, tower, nullptr);
    r.start();
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(r.redFlag());
    r.join();

    const auto s = tower.snapshot();
    for (std::size_t i = 0; i < grid.size(); ++i) {
        SCOPED_TRACE(i);
        if (grid[i].adr()) {
            EXPECT_EQ(s[i].state, CarStatus::State::Retired);
        } else {
            EXPECT_EQ(s[i].state, CarStatus::State::Stopped);
            EXPECT_EQ(grid[i].completed() % perLap, 0u);
            EXPECT_EQ(grid[i].result().sections.size(), grid[i].completed());
        }
    }
}

TEST(RedFlagTest, RefusedOnceACarFinished)
{
    const Track t = shortRace(3);
    auto grid = makeGrid(t);
    TimingTower tower(grid);
    Race r(grid, t, tower, nullptr);
    r.start();
    ASSERT_TRUE(waitFor(tower, [](const CarStatus& c) {
        return c.state == CarStatus::State::Finished;
    }));
    EXPECT_FALSE(r.redFlag());
    r.join();
    for (const CarStatus& c : tower.snapshot())
        EXPECT_NE(c.state, CarStatus::State::Stopped);
}

// Cars stopped after different numbers of laps, run here one by one.
TEST(RedFlagTest, OrderIsLapsThenTimeWithoutRetired)
{
    const Track t = shortRace(3);
    const std::size_t perLap = t.sections.size();
    auto grid = makeGrid(t);
    for (std::size_t i = 0; i < grid.size(); ++i) {
        for (std::size_t k = 0; k < (i % 3) * perLap; ++k)
            grid[i].runNext(t);
    }
    std::size_t retired = 0;
    for (const Car& car : grid)
        retired += car.adr().has_value();
    ASSERT_GT(retired, 0u);  // the cold Newton and Secant crash in the first lap

    const auto order = redFlagOrder(grid, t);
    ASSERT_EQ(order.size(), grid.size() - retired);
    auto car = [&](const GridSlot& s) -> const Car& {
        return *std::find_if(grid.begin(), grid.end(), [&](const Car& c) {
            return c.team() == s.team && c.driver() == s.driver;
        });
    };
    for (std::size_t i = 0; i < order.size(); ++i) {
        const Car& c = car(order[i]);
        EXPECT_FALSE(c.adr().has_value());
        EXPECT_EQ(order[i].laps, c.completed() / perLap);
        if (i > 0) {
            const Car& prev = car(order[i - 1]);
            EXPECT_GE(order[i - 1].laps, order[i].laps);
            if (order[i - 1].laps == order[i].laps) {
                EXPECT_LE(prev.result().total, c.result().total);
            }
        }
    }
}

TEST(RedFlagFileTest, RoundTrip)
{
    const std::vector<GridSlot> order{{Team::Secant, Driver::Warm, 217},
                                      {Team::Halley, Driver::Cold, 197},
                                      {Team::Bisection, Driver::Warm, 0}};
    const std::string path = tempPath("red-flag.bin");
    saveRedFlag(path, order);
    const auto back = loadRedFlag(path);
    ASSERT_EQ(back.size(), order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(back[i].team, order[i].team);
        EXPECT_EQ(back[i].driver, order[i].driver);
        EXPECT_EQ(back[i].laps, order[i].laps);
    }
}

TEST(RedFlagFileTest, BrokenFilesThrow)
{
    EXPECT_THROW(loadRedFlag(tempPath("does-not-exist.bin")), std::runtime_error);

    const std::string text = tempPath("not-a-red-flag");
    std::ofstream(text) << "formulat-track 1\n";
    EXPECT_THROW(loadRedFlag(text), std::runtime_error);

    const std::string v2 = tempPath("red-flag-v2.bin");
    saveRedFlag(v2, {});
    std::string bytes;
    {
        std::ifstream is(v2, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
    }
    ASSERT_GE(bytes.size(), 8u);
    bytes[4] = 2;
    std::ofstream(v2, std::ios::binary) << bytes;
    EXPECT_THROW(loadRedFlag(v2), std::runtime_error);
}

TEST(ResumeTest, GridInTheSavedOrderAfterItsLaps)
{
    const Track t = shortRace(10);
    // Newton Warm finished its last lap under the flag: it doesn't restart.
    const std::vector<GridSlot> order{
        {Team::Newton, Driver::Warm, 10}, {Team::Halley, Driver::Warm, 7}, {Team::Toms748, Driver::Cold, 3}};
    const auto grid = makeGrid(t, order);
    ASSERT_EQ(grid.size(), 2u);
    EXPECT_EQ(grid[0].team(), Team::Halley);
    EXPECT_EQ(grid[0].driver(), Driver::Warm);
    EXPECT_EQ(grid[0].completed(), 7 * t.sections.size());
    EXPECT_EQ(grid[1].team(), Team::Toms748);
    EXPECT_EQ(grid[1].completed(), 3 * t.sections.size());
    EXPECT_TRUE(grid[0].result().sections.empty());  // race times start again
}

// The capstone: red flag, save, load, restart. Each car's sections before the
// flag plus after the restart are the ones the sequential run gives.
TEST(ResumeTest, RedFlagThenRestartMatchesSequential)
{
    const Track t = shortRace(10);
    const std::size_t perLap = t.sections.size();
    auto sequential = makeGrid(t);
    runSequential(sequential, t);

    auto before = makeGrid(t);
    {
        TimingTower tower(before);
        Race r(before, t, tower, nullptr);
        r.start();
        ASSERT_TRUE(waitFor(tower, [&](const CarStatus& c) { return c.completed >= 2 * perLap; }));
        ASSERT_TRUE(r.redFlag());
    }  // the destructor joins

    const std::string path = tempPath("resume.bin");
    saveRedFlag(path, redFlagOrder(before, t));
    auto after = makeGrid(t, loadRedFlag(path));
    {
        TimingTower tower(after);
        Race r(after, t, tower, nullptr);
        r.start();
    }

    auto find = [](const std::vector<Car>& grid, Team team, Driver driver) -> const Car* {
        for (const Car& c : grid)
            if (c.team() == team && c.driver() == driver)
                return &c;
        return nullptr;
    };
    for (const Car& expected : sequential) {
        SCOPED_TRACE(static_cast<int>(expected.team()) * 2 + static_cast<int>(expected.driver()));
        std::vector<SectionResult> sections = find(before, expected.team(), expected.driver())->result().sections;
        if (const Car* resumed = find(after, expected.team(), expected.driver())) {
            const auto& more = resumed->result().sections;
            sections.insert(sections.end(), more.begin(), more.end());
            EXPECT_TRUE(resumed->done());
        }
        const auto& want = expected.result().sections;
        ASSERT_EQ(sections.size(), want.size());
        for (std::size_t j = 0; j < want.size(); ++j) {
            EXPECT_EQ(sections[j].outcome, want[j].outcome);
            EXPECT_EQ(sections[j].pitStop, want[j].pitStop);
        }
    }
}
