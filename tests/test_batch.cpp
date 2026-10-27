#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "options/batch.hpp"
#include "options/binomial.hpp"

using namespace opt;

namespace {

std::vector<Contract> make_chain() {
    std::vector<Contract> chain;
    for (double expiry : {0.1, 0.5, 1.0}) {
        for (double strike = 70.0; strike <= 130.0; strike += 2.5) {
            chain.push_back({OptionType::Call, ExerciseStyle::American, strike, expiry});
            chain.push_back({OptionType::Put, ExerciseStyle::American, strike, expiry});
        }
    }
    return chain;
}

// Throws on one particular strike so we can check errors cross threads.
class FailingPricer : public Pricer {
public:
    double price(const Contract& c, const Market&) const override {
        if (c.strike == 100.0) {
            throw std::runtime_error("bad contract");
        }
        return 1.0;
    }
};

}  // namespace

TEST(Batch, PricesMatchSerialForAnyThreadCount) {
    const BinomialPricer tree(200);
    const Market m{100.0, 0.05, 0.01, 0.3};
    const auto chain = make_chain();

    std::vector<double> serial;
    for (const auto& c : chain) {
        serial.push_back(tree.price(c, m));
    }

    for (unsigned threads : {1u, 2u, 3u, 8u, 0u}) {
        const auto parallel = price_batch(tree, chain, m, threads);
        ASSERT_EQ(parallel.size(), serial.size());
        for (std::size_t i = 0; i < serial.size(); ++i) {
            // Same arithmetic in the same order, so results should be bit-identical.
            EXPECT_EQ(parallel[i], serial[i]) << "threads " << threads << " index " << i;
        }
    }
}

TEST(Batch, ImpliedVolsRoundTrip) {
    const BinomialPricer tree(200);
    Market m{100.0, 0.05, 0.01, 0.0};
    std::vector<Quote> quotes;
    for (const auto& c : make_chain()) {
        // Out-of-the-money options only, like a real IV surface.
        const bool otm = (c.type == OptionType::Call) ? c.strike > m.spot : c.strike < m.spot;
        if (!otm) {
            continue;
        }
        m.volatility = 0.2 + 0.002 * std::abs(c.strike - 100.0);  // a simple smile
        quotes.push_back({c, tree.price(c, m)});
    }

    const auto results = implied_vol_batch(tree, quotes, m, 4);
    ASSERT_EQ(results.size(), quotes.size());
    for (std::size_t i = 0; i < quotes.size(); ++i) {
        const double expected = 0.2 + 0.002 * std::abs(quotes[i].contract.strike - 100.0);
        ASSERT_EQ(results[i].status, IvStatus::Converged) << "index " << i;
        EXPECT_NEAR(results[i].volatility, expected, 1e-4) << "index " << i;
    }
}

TEST(Batch, EmptyInputGivesEmptyOutput) {
    const BinomialPricer tree(50);
    EXPECT_TRUE(price_batch(tree, {}, Market{100.0, 0.05, 0.0, 0.2}, 4).empty());
}

TEST(Batch, WorkerExceptionsReachTheCaller) {
    const FailingPricer pricer;
    const auto chain = make_chain();
    for (unsigned threads : {1u, 4u}) {
        EXPECT_THROW(price_batch(pricer, chain, Market{100.0, 0.05, 0.0, 0.2}, threads), std::runtime_error)
            << "threads " << threads;
    }
}
