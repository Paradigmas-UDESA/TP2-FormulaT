#include "formulat/car.h"

#include <stdexcept>

namespace formulat {

std::queue<std::size_t> makeQueue(const Track& track, std::size_t completed)
{
    (void)track;
    (void)completed;
    throw std::logic_error("sin implementar");
}

Car::Car(Team team, Driver driver, const Track& track, std::size_t completed,
         std::chrono::nanoseconds start)
{
    (void)team;
    (void)driver;
    (void)track;
    (void)completed;
    (void)start;
    throw std::logic_error("sin implementar");
}

Team Car::team() const
{
    throw std::logic_error("sin implementar");
}

Driver Car::driver() const
{
    throw std::logic_error("sin implementar");
}

bool Car::done() const
{
    throw std::logic_error("sin implementar");
}

std::size_t Car::completed() const
{
    throw std::logic_error("sin implementar");
}

void Car::runNext(const Track& track)
{
    (void)track;
    throw std::logic_error("sin implementar");
}

void Car::retire(const std::string& reason)
{
    (void)reason;
    throw std::logic_error("sin implementar");
}

const CarResult& Car::result() const
{
    throw std::logic_error("sin implementar");
}

const std::optional<AccidentDataRecorder>& Car::adr() const
{
    throw std::logic_error("sin implementar");
}

std::vector<Car> makeGrid(const Track& track, std::chrono::nanoseconds gap)
{
    (void)track;
    (void)gap;
    throw std::logic_error("sin implementar");
}

// --- dado -------------------------------------------------------------------

void Car::retireAt(double residual, const std::string& reason)
{
    if (adr_)
        return;  // con abandonar una vez alcanza
    // completed_ cuenta las secciones anteriores a esta, sea cual sea la vuelta.
    result_.finished = false;
    adr_ = AccidentDataRecorder{result_.team,
                                result_.driver,
                                static_cast<unsigned>(completed_ / sectionsPerLap_ + 1),
                                current_,
                                currentType_,
                                result_.total,
                                pitStops_,
                                penalties_,
                                residual,
                                reason};
}

}  // namespace formulat
