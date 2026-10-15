#pragma once

#include <vector>

#include "formulat/car.h"
#include "formulat/track.h"

namespace formulat {

// The race without threads: every car to the end, one after another. An
// engine exception retires the car (retire(what())). Clase 4 runs the same
// loop with one thread per car.
void runSequential(std::vector<Car>& grid, const Track& track);

}  // namespace formulat
