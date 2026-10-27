#pragma once

#include "options/pricer.hpp"
#include "options/types.hpp"

namespace opt {

// Cox-Ross-Rubinstein binomial tree.
class BinomialPricer : public Pricer {
public:
    explicit BinomialPricer(int steps = 500);

    double price(const Contract& contract, const Market& market) const override;

    int steps() const { return steps_; }

private:
    int steps_;
};

}  // namespace opt
