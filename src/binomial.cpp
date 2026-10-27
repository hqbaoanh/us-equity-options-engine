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

// The root price plus the option values at the first two levels of the
// tree, which is all the Greeks need.
struct TreeResult {
    double price;
    double u;
    double dt;
    double v1[2];  // step 1: down, up
    double v2[3];  // step 2: down-down, middle, up-up
};

TreeResult roll_back(const Contract& c, const Market& m, int n) {
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

    TreeResult out{};
    out.u = u;
    out.dt = dt;
    auto keep_early_levels = [&](int step) {
        if (step == 2) {
            std::copy(values.begin(), values.begin() + 3, out.v2);
        } else if (step == 1) {
            std::copy(values.begin(), values.begin() + 2, out.v1);
        }
    };
    keep_early_levels(n);

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
        keep_early_levels(i);
    }
    out.price = values[0];
    return out;
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
    return roll_back(c, m, steps_).price;
}

Greeks BinomialPricer::greeks(const Contract& c, const Market& m) const {
    check_inputs(c, m);
    if (c.expiry == 0.0) {
        throw std::invalid_argument("Greeks need a positive expiry");
    }
    if (steps_ < 2) {
        throw std::invalid_argument("Greeks need at least two tree steps");
    }

    const TreeResult t = roll_back(c, m, steps_);
    const double s = m.spot;
    const double u = t.u;

    Greeks g{};

    // Delta: slope between the two nodes at step 1 (stock at S/u and S*u).
    g.delta = (t.v1[1] - t.v1[0]) / (s * u - s / u);

    // Gamma: how much that slope changes across step 2 (S/u^2, S, S*u^2).
    const double s_up = s * u * u;
    const double s_down = s / (u * u);
    const double delta_up = (t.v2[2] - t.v2[1]) / (s_up - s);
    const double delta_down = (t.v2[1] - t.v2[0]) / (s - s_down);
    g.gamma = (delta_up - delta_down) / (0.5 * (s_up - s_down));

    // The middle node at step 2 has the same stock price as the root,
    // so the change in value there is pure time decay over 2*dt.
    g.theta = (t.v2[1] - t.price) / (2.0 * t.dt);

    // The tree has no direct vega, so bump volatility and reprice.
    const double h = std::min(0.01, 0.5 * m.volatility);
    Market up = m;
    Market down = m;
    up.volatility += h;
    down.volatility -= h;
    g.vega = (roll_back(c, up, steps_).price - roll_back(c, down, steps_).price) / (2.0 * h);

    return g;
}

}  // namespace opt
