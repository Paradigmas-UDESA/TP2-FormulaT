#include "formulat/sequential.h"

#include <exception>

namespace formulat {

void runSequential(std::vector<Car>& grid, const Track& track)
{
    for (Car& car : grid) {
        while (!car.done()) {
            try {
                car.runNext(track);
            } catch (const std::exception& e) {
                car.retire(e.what());
            }
        }
    }
}

}  // namespace formulat
