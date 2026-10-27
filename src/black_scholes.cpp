#include "options/black_scholes.hpp"

#include <cmath>
#include <stdexcept>

namespace opt {

namespace {

constexpr double kPi = 3.14159265358979323846;

double norm_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double norm_pdf(double x) {
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * kPi);
}

struct D1D2 {
    double d1;
    double d2;
};

D1D2 compute_d1_d2(const Market& m, double strike, double expiry) {
    const double vol_sqrt_t = m.volatility * std::sqrt(expiry);
    const double d1 = (std::log(m.spot / strike) +
                       (m.rate - m.dividend_yield + 0.5 * m.volatility * m.volatility) * expiry) /
                      vol_sqrt_t;
    return {d1, d1 - vol_sqrt_t};
}

}  // namespace

double black_scholes_price(OptionType type, const Market& m, double strike, double expiry) {
    if (expiry <= 0.0) {
        return intrinsic_value(type, m.spot, strike);
    }

    const auto [d1, d2] = compute_d1_d2(m, strike, expiry);
    const double fwd_spot = m.spot * std::exp(-m.dividend_yield * expiry);
    const double pv_strike = strike * std::exp(-m.rate * expiry);

    if (type == OptionType::Call) {
        return fwd_spot * norm_cdf(d1) - pv_strike * norm_cdf(d2);
    }
    return pv_strike * norm_cdf(-d2) - fwd_spot * norm_cdf(-d1);
}

Greeks black_scholes_greeks(OptionType type, const Market& m, double strike, double expiry) {
    if (expiry <= 0.0) {
        throw std::invalid_argument("Greeks need a positive expiry");
    }
    const auto [d1, d2] = compute_d1_d2(m, strike, expiry);
    const double sqrt_t = std::sqrt(expiry);
    const double div_disc = std::exp(-m.dividend_yield * expiry);
    const double rate_disc = std::exp(-m.rate * expiry);
    const double pdf_d1 = norm_pdf(d1);

    Greeks g{};
    g.gamma = div_disc * pdf_d1 / (m.spot * m.volatility * sqrt_t);
    g.vega = m.spot * div_disc * pdf_d1 * sqrt_t;

    const double decay = -m.spot * div_disc * pdf_d1 * m.volatility / (2.0 * sqrt_t);
    if (type == OptionType::Call) {
        g.delta = div_disc * norm_cdf(d1);
        g.theta = decay - m.rate * strike * rate_disc * norm_cdf(d2) +
                  m.dividend_yield * m.spot * div_disc * norm_cdf(d1);
    } else {
        g.delta = div_disc * (norm_cdf(d1) - 1.0);
        g.theta = decay + m.rate * strike * rate_disc * norm_cdf(-d2) -
                  m.dividend_yield * m.spot * div_disc * norm_cdf(-d1);
    }
    return g;
}

double BlackScholesPricer::price(const Contract& c, const Market& m) const {
    if (c.style != ExerciseStyle::European) {
        throw std::invalid_argument("Black-Scholes only prices European options");
    }
    return black_scholes_price(c.type, m, c.strike, c.expiry);
}

}  // namespace opt
