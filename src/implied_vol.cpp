#include "options/implied_vol.hpp"

#include <algorithm>
#include <cmath>

namespace opt {

IvResult implied_volatility(const Pricer& pricer, const Contract& contract, Market market,
                            double target_price, const IvOptions& opts) {
    auto error_at = [&](double vol) {
        market.volatility = vol;
        return pricer.price(contract, market) - target_price;
    };

    double lo = opts.vol_low;
    double hi = opts.vol_high;

    // Option prices increase with volatility, so a root exists only if the
    // target sits between the prices at the two ends of the range.
    // If the price at the lowest volatility already matches, the price is
    // flat in volatility there (usually an option worth exactly its exercise
    // value), so any low volatility fits and there's no single answer.
    const double err_lo = error_at(lo);
    if (std::abs(err_lo) < opts.price_tolerance) {
        return {lo, 0, IvStatus::Undetermined};
    }
    const double err_hi = error_at(hi);
    if (err_lo > 0.0 || err_hi < 0.0) {
        return {0.0, 0, IvStatus::NoSolution};
    }

    double vol = std::clamp(0.3, lo, hi);

    for (int iter = 1; iter <= opts.max_iterations; ++iter) {
        const double err = error_at(vol);
        if (std::abs(err) < opts.price_tolerance) {
            return {vol, iter, IvStatus::Converged};
        }

        // The root is on the side where the error changes sign.
        if (err < 0.0) {
            lo = vol;
        } else {
            hi = vol;
        }
        if (hi - lo < 1e-12) {
            return {vol, iter, IvStatus::Converged};
        }

        // Error and price differ by a constant, so this is also the vega.
        const double bump = std::min(1e-3, 0.5 * vol);
        const double vega = (error_at(vol + bump) - error_at(vol - bump)) / (2.0 * bump);
        double next = vol - err / vega;
        // Written as !(vega > ...) so a NaN vega also falls back to bisection.
        if (!(vega > 1e-8) || next <= lo || next >= hi) {
            next = 0.5 * (lo + hi);
        }
        vol = next;
    }
    return {vol, opts.max_iterations, IvStatus::NotConverged};
}

}  // namespace opt
