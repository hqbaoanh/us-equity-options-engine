#pragma once

#include <vector>

#include "options/implied_vol.hpp"
#include "options/pricer.hpp"
#include "options/types.hpp"

namespace opt {

struct Quote {
    Contract contract;
    double market_price;
};

// Every contract is priced independently, so these split the work across
// threads with no shared state besides a work counter. threads = 0 uses all
// hardware threads. The pricer must be safe to call from several threads at
// once, which holds for any pricer whose price() only reads its members.

std::vector<double> price_batch(const Pricer& pricer, const std::vector<Contract>& contracts,
                                const Market& market, unsigned threads = 0);

std::vector<IvResult> implied_vol_batch(const Pricer& pricer, const std::vector<Quote>& quotes,
                                        const Market& market, unsigned threads = 0,
                                        const IvOptions& options = {});

}  // namespace opt
