#pragma once

#include <cstddef>
#include <vector>

namespace formulat {

// p(x) = c0 + c1 x + ... + cn x^n, coefficients in ascending order.
class Polynomial {
public:
    // Throws std::invalid_argument if coeffs is empty. Trailing zeros are kept.
    explicit Polynomial(std::vector<double> coeffs);

    // Horner evaluation.
    double operator()(double x) const;

    // The derivative of a constant is the constant 0.
    Polynomial derivative() const;

    // coefficients().size() - 1, even if the leading coefficient is 0.
    std::size_t degree() const;

    const std::vector<double>& coefficients() const;

private:
    std::vector<double> coeffs_;
};

}  // namespace formulat
