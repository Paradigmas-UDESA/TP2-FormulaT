#include "formulat/redflag.h"

#include <stdexcept>

// TODO (clase 5): implementar cada función. Ver include/formulat/redflag.h.

namespace formulat {

std::vector<GridSlot> redFlagOrder(const std::vector<Car>& grid, const Track& track)
{
    (void)grid;
    (void)track;
    throw std::logic_error("sin implementar");
}

void saveRedFlag(const std::string& path, const std::vector<GridSlot>& order)
{
    (void)path;
    (void)order;
    throw std::logic_error("sin implementar");
}

std::vector<GridSlot> loadRedFlag(const std::string& path)
{
    (void)path;
    throw std::logic_error("sin implementar");
}

std::vector<Car> makeGrid(const Track& track, const std::vector<GridSlot>& order)
{
    (void)track;
    (void)order;
    throw std::logic_error("sin implementar");
}

}  // namespace formulat
