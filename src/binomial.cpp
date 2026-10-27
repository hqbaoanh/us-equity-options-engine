#include "options/binomial.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace opt {

namespace {

void check_inputs(const Contract& c, const Market& m) {
    if (m.spot <= 0.0 || c.strike <= 0.0) {
        throw std::invalid_argument("spot and strike must be positive");
    }
    if (m.volatility <= 0.0) {
        throw std::invalid_argument("volatility must be positive");
    }
    if (c.expiry < 0.0) {
        throw std::invalid_argument("expiry must be non-negative");
    }
}

}  // namespace

BinomialPricer::BinomialPricer(int steps) : steps_(steps) {
    if (steps < 1) {
        throw std::invalid_argument("tree needs at least one step");
    }
}

double BinomialPricer::price(const Contract& c, const Market& m) const {
    check_inputs(c, m);
    if (c.expiry == 0.0) {
        return intrinsic_value(c.type, m.spot, c.strike);
    }

    const int n = steps_;
    const double dt = c.expiry / n;
    const double u = std::exp(m.volatility * std::sqrt(dt));
    const double d = 1.0 / u;
    const double p = (std::exp((m.rate - m.dividend_yield) * dt) - d) / (u - d);
    if (p <= 0.0 || p >= 1.0) {
        throw std::domain_error("risk-neutral probability outside (0, 1); use more steps");
    }

    // Fold the per-step discount into the probabilities.
    const double disc = std::exp(-m.rate * dt);
    const double pu = disc * p;
    const double pd = disc * (1.0 - p);

    // values[j] is the option value at the node with j up-moves.
    // At expiry that node's stock price is S * u^j * d^(n-j) = S * u^(2j-n).
    std::vector<double> values(n + 1);
    for (int j = 0; j <= n; ++j) {
        const double s = m.spot * std::pow(u, 2 * j - n);
        values[j] = intrinsic_value(c.type, s, c.strike);
    }

    // Roll back one step at a time, reusing the same buffer.
    const bool american = (c.style == ExerciseStyle::American);
    const double u2 = u * u;
    for (int i = n - 1; i >= 0; --i) {
        double s = m.spot * std::pow(d, i);  // lowest node at step i
        for (int j = 0; j <= i; ++j) {
            const double hold = pu * values[j + 1] + pd * values[j];
            values[j] = american ? std::max(hold, intrinsic_value(c.type, s, c.strike)) : hold;
            s *= u2;
        }
    }
    return values[0];
}

}  // namespace opt
