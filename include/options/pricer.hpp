#pragma once

#include "options/types.hpp"

namespace opt {

// Common interface so the IV solver, Greeks and batch code don't care
// which model produced the price.
class Pricer {
public:
    virtual ~Pricer() = default;
    virtual double price(const Contract& contract, const Market& market) const = 0;
};

}  // namespace opt
