#pragma once

#include "options/greeks.hpp"
#include "options/pricer.hpp"
#include "options/types.hpp"

namespace opt {

// Cox-Ross-Rubinstein binomial tree.
class BinomialPricer : public Pricer {
public:
    explicit BinomialPricer(int steps = 500);

    double price(const Contract& contract, const Market& market) const override;

    // Delta, gamma and theta come from the tree's first nodes at no extra
    // cost; vega needs two more pricings with volatility bumped.
    Greeks greeks(const Contract& contract, const Market& market) const;

    int steps() const { return steps_; }

private:
    int steps_;
};

}  // namespace opt
