#pragma once

#include "options/types.hpp"

namespace opt {

// IV solver, batch code and bindings only see this, not the tree
class Pricer {
public:
    virtual ~Pricer() = default;
    virtual double price(const Contract& contract, const Market& market) const = 0;
};

}  // namespace opt
