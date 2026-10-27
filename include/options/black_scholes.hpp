#pragma once

#include "options/pricer.hpp"
#include "options/types.hpp"

namespace opt {

// Closed-form European prices. Used as the reference the tree is tested against.
double black_scholes_price(OptionType type, const Market& market, double strike, double expiry);

class BlackScholesPricer : public Pricer {
public:
    // Throws std::invalid_argument for American contracts.
    double price(const Contract& contract, const Market& market) const override;
};

}  // namespace opt
