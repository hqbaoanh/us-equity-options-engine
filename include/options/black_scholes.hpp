#pragma once

#include "options/greeks.hpp"
#include "options/pricer.hpp"
#include "options/types.hpp"

namespace opt {

// closed-form European price and Greeks (reference for testing the tree)
double black_scholes_price(OptionType type, const Market& market, double strike, double expiry);
Greeks black_scholes_greeks(OptionType type, const Market& market, double strike, double expiry);

class BlackScholesPricer : public Pricer {
public:
    // Throws std::invalid_argument for American contracts.
    double price(const Contract& contract, const Market& market) const override;
};

}  // namespace opt
