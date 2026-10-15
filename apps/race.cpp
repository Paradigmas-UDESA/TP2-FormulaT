// The race: one thread per car. The cars log their events (stamped with the
// time since lights out); the main thread shows the lights, then the leader
// once a second, then the classification, and checks the tower lost nothing.
//
//   build/race [track file] [--red-flag <seconds>] [--resume <file>]
//
//   build/race                            (default: tracks/formula-t.track)
//   build/race tracks/stress.track        (tiny sections, many laps: races show up)
//   build/race --red-flag 5               (stops the race 5 s after lights out and
//                                          saves the order to results/red-flag.bin)
//   build/race --resume results/red-flag.bin   (restarts from that order; use the
//                                               same track)
//
// For the GUI (gui/serve.py), every 100 ms: results/live.bin (the tower's
// snapshot, binary), and results/track.txt once (the circuit). A file
// results/red-flag.request (the GUI's red flag button) waves the red flag.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "formulat/car.h"
#include "formulat/race.h"
#include "formulat/timingtower.h"
#include "formulat/track.h"
#include "formulat/binary.h"
#include "formulat/qualifying.h"
#include "formulat/redflag.h"
#include "formulat/serialize.h"

using namespace formulat;

namespace {

template <typename E>
std::string nameOf(E value)
{
    std::ostringstream os;
    write(os, value);
    return os.str();
}

std::string carName(const CarStatus& s)
{
    return nameOf(s.team) + " " + nameOf(s.driver);
}

// gui/app.js reads CarStatus as raw bytes: this layout is its contract.
static_assert(sizeof(CarStatus) == 32, "CarStatus layout changed: update gui/app.js");

const char* const kLive = "results/live.bin";
const char* const kLiveTmp = "results/live.bin.tmp";
const char* const kRequest = "results/red-flag.request";

// "FTLV", a 32-bit version (1), the time since lights out, the cars. Written
// aside and renamed over the old file (atomic): the GUI never reads half a file.
void writeLive(std::chrono::nanoseconds elapsed, const std::vector<CarStatus>& cars)
{
    {
        std::ofstream os(kLiveTmp, std::ios::binary);
        os.write("FTLV", 4);
        writeBinary(os, std::uint32_t{1});
        writeBinary(os, elapsed);
        writeBinary(os, cars);
    }
    std::filesystem::rename(kLiveTmp, kLive);
}

}  // namespace

int main(int argc, char** argv)
{
    using namespace std::chrono_literals;
    std::string path = "tracks/formula-t.track";
    double redFlagAfter = -1;  // seconds; < 0: no red flag
    std::string resume;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--red-flag" && i + 1 < argc)
                redFlagAfter = std::stod(argv[++i]);
            else if (arg == "--resume" && i + 1 < argc)
                resume = argv[++i];
            else
                path = arg;
        }

        const Track track = generate(loadTrackConfig(path));
        std::filesystem::create_directories("results");
        saveTrackConfig("results/track.txt", track.config);
        std::filesystem::remove(kRequest);  // a leftover would stop this race at once
        std::cout << track.config.name << ": " << track.sections.size() << " sections, "
                  << track.config.laps << " laps; " << std::thread::hardware_concurrency()
                  << " cores for 12 cars\n\n";

        // Complete BEFORE the threads start.
        std::vector<Car> grid = resume.empty() ? makeGrid(track) : makeGrid(track, loadRedFlag(resume));
        if (!resume.empty())
            std::cout << "restart from " << resume << " (race times start again at 0)\n";
        TimingTower tower(grid);
        const StartProcedure procedure{1s, 50ms};
        Race race(grid, track, tower, &std::cout, procedure);
        race.start();

        const std::size_t perLap = track.sections.size();
        bool redFlagWaved = false;
        auto wave = [&](const char* why) {
            redFlagWaved = true;
            std::cout << (race.redFlag() ? std::string("-- RED FLAG (") + why +
                                               "): every car stops at the end of its lap\n"
                                         : "-- red flag refused: a car already finished\n");
        };
        int lightsShown = 0;
        for (unsigned tick = 0; !tower.allDone(); ++tick) {
            // Copy under the lock, work outside it (the console, the file).
            const auto cars = tower.snapshot();
            const auto elapsed = race.elapsed();
            writeLive(elapsed, cars);

            if (elapsed < 0ns) {
                // One more light every interval, all out at 0.
                const double e = std::chrono::duration<double>(elapsed).count() /
                                 std::chrono::duration<double>(procedure.lightInterval).count();
                const int lights = std::clamp(6 + static_cast<int>(std::floor(e)), 1, 5);
                if (lights != lightsShown) {
                    lightsShown = lights;
                    std::cout << "-- lights " << std::string(lights, '*')
                              << std::string(5 - lights, '.') << '\n';
                }
            } else {
                if (std::filesystem::remove(kRequest) && !redFlagWaved)
                    wave("GUI");
                if (!redFlagWaved && redFlagAfter >= 0 &&
                    elapsed >= std::chrono::duration<double>(redFlagAfter))
                    wave("--red-flag");
                if (tick % 10 == 0) {
                    const auto leader = std::max_element(
                        cars.begin(), cars.end(), [](const CarStatus& a, const CarStatus& b) {
                            return a.completed < b.completed;
                        });
                    const auto running =
                        std::count_if(cars.begin(), cars.end(), [](const CarStatus& s) {
                            return s.state == CarStatus::State::Running;
                        });
                    const std::size_t lap =
                        std::min<std::size_t>(leader->completed / perLap + 1, track.config.laps);
                    std::cout << "-- " << running << " running; leader " << carName(*leader)
                              << ", lap " << lap << '\n';
                }
            }
            std::this_thread::sleep_for(100ms);
        }
        race.join();
        writeLive(race.elapsed(), tower.snapshot());

        const auto status = tower.snapshot();
        if (std::any_of(status.begin(), status.end(), [](const CarStatus& s) {
                return s.state == CarStatus::State::Stopped;
            })) {
            const auto order = redFlagOrder(grid, track);
            saveRedFlag("results/red-flag.bin", order);
            std::printf("\nrestart grid (saved results/red-flag.bin):\n");
            for (std::size_t i = 0; i < order.size(); ++i) {
                const std::string name = nameOf(order[i].team) + " " + nameOf(order[i].driver);
                std::printf("%-4zu %-24s lap %zu\n", i + 1, name.c_str(), order[i].laps);
            }
            return 0;
        }

        // The final classification is by race time, penalties included (like
        // F1, it can differ from the order the cars crossed the line). After a
        // restart only the order matters: by who crossed the line.
        std::vector<CarResult> results;
        for (const Car& car : grid)
            results.push_back(car.result());
        auto position = [&](const CarResult& r) -> unsigned {
            for (const CarStatus& s : status)
                if (s.team == r.team && s.driver == r.driver)
                    return s.position;
            return 0;
        };
        auto crossed = [&](const CarResult& r) {
            const unsigned p = position(r);
            return p ? std::to_string(p) : std::string("-");
        };
        std::vector<CarResult> order = classify(results);
        if (!resume.empty()) {
            // Stable: DNFs (position 0) stay last, in classify's order.
            std::stable_sort(order.begin(), order.end(), [&](const CarResult& a, const CarResult& b) {
                const unsigned pa = position(a), pb = position(b);
                return pa && (!pb || pa < pb);
            });
        }
        std::printf("\n%-4s %-24s %12s %8s\n", "pos", "car",
                    resume.empty() ? "time (s)" : "since restart", "crossed");
        std::size_t pos = 0;
        for (const CarResult& r : order) {
            const std::string name = nameOf(r.team) + " " + nameOf(r.driver);
            const std::string p = r.finished ? std::to_string(++pos) : "DNF";
            std::printf("%-4s %-24s %12.3f %8s\n", p.c_str(), name.c_str(),
                        std::chrono::duration<double>(r.total).count(), crossed(r).c_str());
        }

        // Every section a car ran was reported once, including one that threw
        // (the car retired, nothing was added to its result): the tower must agree.
        std::size_t expected = 0;
        for (const Car& car : grid) {
            const bool threw = car.adr() && car.result().sections.size() == car.completed();
            expected += car.result().sections.size() + (threw ? 1 : 0);
        }
        const std::size_t counted = tower.sectionsRun();
        std::printf("\nsections run: tower %zu, cars %zu", counted, expected);
        if (counted != expected)
            std::printf("  <-- %zu LOST", expected - counted);
        std::printf("\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
