#pragma once

#include "options/pricer.hpp"
#include "options/types.hpp"

namespace opt {

enum class IvStatus {
    Converged,
    NoSolution,    // price is outside what any volatility in the search range produces
    Undetermined,  // even the lowest volatility fits, e.g. an American put priced at K - S
    NotConverged,  // hit the iteration limit
};

struct IvResult {
    double volatility;
    int iterations;
    IvStatus status;
};

struct IvOptions {
    // below ~1% a tree with high rate + few steps gets p > 1, so start here
    double vol_low = 0.01;
    double vol_high = 5.0;
    double price_tolerance = 1e-6;
    int max_iterations = 100;
};

// vol where pricer gives target_price. market.volatility is ignored (used as scratch).
// Newton inside a shrinking bracket; bisection step if Newton leaves the bracket or vega
// is tiny. bisection = nested interval theorem, so it can't fail if a root exists.
IvResult implied_volatility(const Pricer& pricer, const Contract& contract, Market market,
                            double target_price, const IvOptions& options = {});

}  // namespace opt
