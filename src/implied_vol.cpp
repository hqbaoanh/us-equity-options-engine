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

    // price is increasing in vol -> root exists iff target is between the prices at lo and hi (IVT).
    // if lo already matches, price is flat in vol there (e.g. put worth exactly K - S),
    // so the root isn't unique.
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

        // keep the half where the sign changes
        if (err < 0.0) {
            lo = vol;
        } else {
            hi = vol;
        }
        if (hi - lo < 1e-12) {
            return {vol, iter, IvStatus::Converged};
        }

        // d(error)/d(vol) = vega
        const double bump = std::min(1e-3, 0.5 * vol);
        const double vega = (error_at(vol + bump) - error_at(vol - bump)) / (2.0 * bump);
        double next = vol - err / vega;
        // !(vega > ...) so a NaN vega also falls back to bisection
        if (!(vega > 1e-8) || next <= lo || next >= hi) {
            next = 0.5 * (lo + hi);
        }
        vol = next;
    }
    return {vol, opts.max_iterations, IvStatus::NotConverged};
}

}  // namespace opt
