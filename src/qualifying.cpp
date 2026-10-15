#include "formulat/qualifying.h"

#include <algorithm>
#include <exception>
#include <limits>

namespace formulat {

namespace {

std::size_t completed(const CarResult& car)
{
    return std::count_if(car.sections.begin(), car.sections.end(),
                         [](const SectionResult& r) { return r.outcome != Outcome::DNF; });
}

}  // namespace

Qualifying qualify(const Track& track)
{
    const Team teams[] = {Team::Bisection, Team::Toms748, Team::Newton,
                          Team::Halley,    Team::Secant,  Team::RegulaFalsi};
    const TrackConfig& cfg = track.config;
    auto fallback = makeFallback();

    Qualifying q;
    for (Team team : teams) {
        for (Driver driver : {Driver::Cold, Driver::Warm}) {
            auto engine = makeEngine(team);
            CarResult car{team, driver, true, std::chrono::nanoseconds{0}, {}};
            unsigned pits = 0;
            unsigned penalties = 0;
            for (std::size_t i = 0; i < track.sections.size(); ++i) {
                const Section& s = track.sections[i];
                auto retire = [&](double residual, std::string reason) {
                    car.finished = false;
                    q.adrs.push_back({team, driver, 1, i, s.type, car.total, pits, penalties,
                                      residual, std::move(reason)});
                };
                try {
                    const SectionResult r = runSection(*engine, *fallback, s, driver, cfg.eps,
                                                       cfg.penaltyPct);
                    car.sections.push_back(r);
                    car.total += r.time;
                    pits += r.pitStop;
                    penalties += r.outcome == Outcome::Penalty;
                    if (r.outcome == Outcome::DNF) {
                        retire(r.residual, refereeVerdict(r, cfg.eps));
                        break;
                    }
                } catch (const std::exception& e) {
                    retire(std::numeric_limits<double>::quiet_NaN(), e.what());
                    break;
                }
            }
            q.cars.push_back(std::move(car));
        }
    }
    return q;
}

std::vector<CarResult> classify(std::vector<CarResult> cars)
{
    std::stable_sort(cars.begin(), cars.end(), [](const CarResult& x, const CarResult& y) {
        if (x.finished != y.finished)
            return x.finished;
        if (!x.finished && completed(x) != completed(y))
            return completed(x) > completed(y);
        return x.total < y.total;
    });
    return cars;
}

}  // namespace formulat
