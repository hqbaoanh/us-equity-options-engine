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

// threads = 0 -> all hardware threads.
// price() gets called from several threads at once, so it must only read the pricer's members.

std::vector<double> price_batch(const Pricer& pricer, const std::vector<Contract>& contracts,
                                const Market& market, unsigned threads = 0);

std::vector<IvResult> implied_vol_batch(const Pricer& pricer, const std::vector<Quote>& quotes,
                                        const Market& market, unsigned threads = 0,
                                        const IvOptions& options = {});

}  // namespace opt
