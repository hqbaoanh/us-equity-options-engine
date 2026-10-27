#include "options/black_scholes.hpp"

#include <cmath>
#include <stdexcept>

namespace opt {

namespace {

double norm_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

}  // namespace

double black_scholes_price(OptionType type, const Market& m, double strike, double expiry) {
    if (expiry <= 0.0) {
        return intrinsic_value(type, m.spot, strike);
    }

    const double vol_sqrt_t = m.volatility * std::sqrt(expiry);
    const double d1 = (std::log(m.spot / strike) +
                       (m.rate - m.dividend_yield + 0.5 * m.volatility * m.volatility) * expiry) /
                      vol_sqrt_t;
    const double d2 = d1 - vol_sqrt_t;

    const double fwd_spot = m.spot * std::exp(-m.dividend_yield * expiry);
    const double pv_strike = strike * std::exp(-m.rate * expiry);

    if (type == OptionType::Call) {
        return fwd_spot * norm_cdf(d1) - pv_strike * norm_cdf(d2);
    }
    return pv_strike * norm_cdf(-d2) - fwd_spot * norm_cdf(-d1);
}

double BlackScholesPricer::price(const Contract& c, const Market& m) const {
    if (c.style != ExerciseStyle::European) {
        throw std::invalid_argument("Black-Scholes only prices European options");
    }
    return black_scholes_price(c.type, m, c.strike, c.expiry);
}

}  // namespace opt
