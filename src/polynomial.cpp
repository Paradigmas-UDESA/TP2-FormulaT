#include "formulat/polynomial.h"

#include <stdexcept>
#include <utility>

namespace formulat {

Polynomial::Polynomial(std::vector<double> coeffs) : coeffs_(std::move(coeffs))
{
    if (coeffs_.empty())
        throw std::invalid_argument("Polynomial: no coefficients");
}

double Polynomial::operator()(double x) const
{
    double y = 0.0;
    for (auto c = coeffs_.rbegin(); c != coeffs_.rend(); ++c)
        y = y * x + *c;
    return y;
}

Polynomial Polynomial::derivative() const
{
    if (coeffs_.size() == 1)
        return Polynomial({0.0});
    std::vector<double> d(coeffs_.size() - 1);
    for (std::size_t i = 1; i < coeffs_.size(); ++i)
        d[i - 1] = static_cast<double>(i) * coeffs_[i];
    return Polynomial(std::move(d));
}

std::size_t Polynomial::degree() const
{
    return coeffs_.size() - 1;
}

const std::vector<double>& Polynomial::coefficients() const
{
    return coeffs_;
}

}  // namespace formulat
