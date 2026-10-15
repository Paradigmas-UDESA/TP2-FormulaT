#include "formulat/timingtower.h"

#include <stdexcept>

// TODO (clase 5): stop(), y proteger todo con un std::mutex.

namespace formulat {

TimingTower::TimingTower(const std::vector<Car>& grid)
{
    (void)grid;
    throw std::logic_error("sin implementar");
}

void TimingTower::report(std::size_t car, std::size_t completed, std::chrono::nanoseconds total)
{
    (void)car;
    (void)completed;
    (void)total;
    throw std::logic_error("sin implementar");
}

unsigned TimingTower::finish(std::size_t car)
{
    (void)car;
    throw std::logic_error("sin implementar");
}

void TimingTower::retire(std::size_t car)
{
    (void)car;
    throw std::logic_error("sin implementar");
}

void TimingTower::stop(std::size_t car)
{
    (void)car;
    throw std::logic_error("sin implementar");
}

std::vector<CarStatus> TimingTower::snapshot() const
{
    throw std::logic_error("sin implementar");
}

bool TimingTower::allDone() const
{
    throw std::logic_error("sin implementar");
}

std::size_t TimingTower::sectionsRun() const
{
    throw std::logic_error("sin implementar");
}

}  // namespace formulat
