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
    // Below about 1% a tree with a high rate and few steps can't keep its
    // up-move larger than the drift, so the search starts there.
    double vol_low = 0.01;
    double vol_high = 5.0;
    double price_tolerance = 1e-6;
    int max_iterations = 100;
};

// Finds the volatility at which pricer reproduces target_price.
// market.volatility is ignored and used only as scratch space.
//
// Newton's method with vega from a central difference, kept inside a
// shrinking bracket. Any step that would leave the bracket, or where vega is
// too small to trust, is replaced by a bisection step, so it always converges
// when a solution exists.
IvResult implied_volatility(const Pricer& pricer, const Contract& contract, Market market,
                            double target_price, const IvOptions& options = {});

}  // namespace opt
