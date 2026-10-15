// Qualifying: every car runs one lap of the track, one after another. Prints
// the classification and saves the results twice: results/qualifying.txt (text,
// for people) and results/qualifying.bin (binary, clase 1: the GUI reads it),
// plus an accident data recorder per DNF
// (results/adr-<team>-<driver>.txt), and the circuit it ran for the GUI
// (results/track.txt).
//
//   build/qualifying [track file]      (default: tracks/formula-t.track)

#include <cstdio>
#include <exception>
#include <filesystem>
#include <sstream>
#include <string>

#include "formulat/track.h"
#include "formulat/binary.h"
#include "formulat/qualifying.h"
#include "formulat/serialize.h"

using namespace formulat;

namespace {

// "Toms748", "Warm", ...: the names the files use.
template <typename E>
std::string nameOf(E value)
{
    std::ostringstream os;
    write(os, value);
    return os.str();
}

}  // namespace

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "tracks/formula-t.track";
    try {
        const Track track = generate(loadTrackConfig(path));
        std::printf("%s: %zu sections\n\n", track.config.name.c_str(), track.sections.size());

        const Qualifying q = qualify(track);
        const std::vector<CarResult> grid = classify(q.cars);

        std::printf("%-4s %-24s %12s %6s %10s\n", "pos", "car", "time (ms)", "pits", "penalties");
        for (std::size_t i = 0; i < grid.size(); ++i) {
            const CarResult& car = grid[i];
            unsigned pits = 0, penalties = 0;
            for (const SectionResult& r : car.sections) {
                pits += r.pitStop;
                penalties += r.outcome == Outcome::Penalty;
            }
            const std::string name = nameOf(car.team) + " " + nameOf(car.driver);
            if (car.finished)
                std::printf("%-4zu %-24s %12.3f %6u %10u\n", i + 1, name.c_str(),
                            car.total.count() / 1e6, pits, penalties);
            else
                std::printf("%-4s %-24s %12s %6u %10u\n", "DNF", name.c_str(), "-", pits, penalties);
        }

        std::filesystem::create_directories("results");
        saveTrackConfig("results/track.txt", track.config);  // the circuit, for the GUI
        saveQualifying("results/qualifying.txt", grid);
        std::printf("\nsaved results/qualifying.txt\n");
        saveQualifyingBinary("results/qualifying.bin", grid);
        std::printf("saved results/qualifying.bin\n");
        for (const AccidentDataRecorder& adr : q.adrs) {
            const std::string file =
                "results/adr-" + nameOf(adr.team) + "-" + nameOf(adr.driver) + ".txt";
            saveAdr(file, adr);
            std::printf("saved %s (%s)\n", file.c_str(), adr.reason.c_str());
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
