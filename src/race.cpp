#include "formulat/race.h"

#include <stdexcept>

namespace formulat {

Race::Race(std::vector<Car>& grid, const Track& track, TimingTower& tower, std::ostream* log,
           std::chrono::nanoseconds lightInterval)
{
    (void)grid;
    (void)track;
    (void)tower;
    (void)log;
    (void)lightInterval;
    throw std::logic_error("sin implementar");
}

void Race::start()
{
    throw std::logic_error("sin implementar");
}

bool Race::redFlag()
{
    // TODO (clase 5)
    throw std::logic_error("sin implementar");
}

void Race::join()
{
    throw std::logic_error("sin implementar");
}

std::chrono::nanoseconds Race::elapsed() const
{
    throw std::logic_error("sin implementar");
}

Race::~Race()
{
}

}  // namespace formulat
