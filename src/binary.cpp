#include "formulat/binary.h"

#include <stdexcept>

namespace formulat {

// --- clase 1 ---------------------------------------------------------------

void CarResult::serialize(std::ostream& out) const
{
    (void)out;
    throw std::logic_error("sin implementar");
}

void CarResult::deserialize(std::istream& in)
{
    (void)in;
    throw std::logic_error("sin implementar");
}

void saveQualifyingBinary(const std::string& path, const std::vector<CarResult>& cars)
{
    (void)path;
    (void)cars;
    throw std::logic_error("sin implementar");
}

std::vector<CarResult> loadQualifyingBinary(const std::string& path)
{
    (void)path;
    throw std::logic_error("sin implementar");
}

// --- clase 2 ---------------------------------------------------------------

void writeBinary(std::ostream& os, const CarResult& value)
{
    (void)os;
    (void)value;
    throw std::logic_error("sin implementar");
}

void readBinary(std::istream& is, CarResult& value)
{
    (void)is;
    (void)value;
    throw std::logic_error("sin implementar");
}

}  // namespace formulat
