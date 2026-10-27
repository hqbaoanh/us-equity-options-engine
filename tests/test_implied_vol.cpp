#include <gtest/gtest.h>

#include <cmath>

#include "options/binomial.hpp"
#include "options/black_scholes.hpp"
#include "options/implied_vol.hpp"

using namespace opt;

namespace {

const char* name(OptionType type) {
    return type == OptionType::Call ? "call" : "put";
}

}  // namespace

TEST(ImpliedVol, RecoversVolatilityFromTreePrices) {
    // Price at a known volatility, then check the solver gets it back.
    BinomialPricer tree(300);
    Market m{100.0, 0.045, 0.01, 0.0};
    for (ExerciseStyle style : {ExerciseStyle::European, ExerciseStyle::American}) {
        for (OptionType type : {OptionType::Call, OptionType::Put}) {
            for (double strike : {75.0, 95.0, 100.0, 110.0, 130.0}) {
                for (double vol : {0.12, 0.25, 0.5, 1.1}) {
                    const Contract c{type, style, strike, 0.5};
                    m.volatility = vol;
                    const double target = tree.price(c, m);

                    const IvResult r = implied_volatility(tree, c, m, target);
                    SCOPED_TRACE(testing::Message() << name(type) << " K=" << strike << " vol=" << vol);

                    // A deep in-the-money American put can be worth exactly K - S
                    // for a whole range of low volatilities, so no single answer exists.
                    if (std::abs(target - intrinsic_value(type, m.spot, strike)) < 1e-9) {
                        EXPECT_EQ(r.status, IvStatus::Undetermined);
                        continue;
                    }
                    ASSERT_EQ(r.status, IvStatus::Converged);
                    EXPECT_NEAR(r.volatility, vol, 1e-4);
                }
            }
        }
    }
}

TEST(ImpliedVol, WorksWithAnyPricer) {
    BlackScholesPricer bs;
    const Contract c{OptionType::Call, ExerciseStyle::European, 105.0, 1.0};
    const Market m{100.0, 0.05, 0.0, 0.0};
    const double target = black_scholes_price(OptionType::Call, Market{100.0, 0.05, 0.0, 0.37}, 105.0, 1.0);

    const IvResult r = implied_volatility(bs, c, m, target);
    ASSERT_EQ(r.status, IvStatus::Converged);
    EXPECT_NEAR(r.volatility, 0.37, 1e-6);
}

TEST(ImpliedVol, NewtonConvergesQuicklyNearTheMoney) {
    BinomialPricer tree(300);
    const Contract c{OptionType::Put, ExerciseStyle::American, 100.0, 0.5};
    const Market m{100.0, 0.05, 0.0, 0.22};
    const IvResult r = implied_volatility(tree, c, m, tree.price(c, m));
    ASSERT_EQ(r.status, IvStatus::Converged);
    EXPECT_LE(r.iterations, 8);
}

TEST(ImpliedVol, HandlesShortDatedFarOutOfTheMoney) {
    // Vega is tiny here, which is where plain Newton tends to overshoot.
    BinomialPricer tree(300);
    const Contract c{OptionType::Call, ExerciseStyle::American, 130.0, 0.05};
    const Market m{100.0, 0.05, 0.0, 0.9};
    const IvResult r = implied_volatility(tree, c, m, tree.price(c, m));
    ASSERT_EQ(r.status, IvStatus::Converged);
    EXPECT_NEAR(r.volatility, 0.9, 1e-3);
}

TEST(ImpliedVol, ReportsNoSolutionOutsideArbitrageBounds) {
    BinomialPricer tree(300);
    const Market m{100.0, 0.05, 0.0, 0.0};

    // An American put can always be exercised for K - S = 20, so 15 is impossible.
    const Contract put{OptionType::Put, ExerciseStyle::American, 120.0, 0.5};
    EXPECT_EQ(implied_volatility(tree, put, m, 15.0).status, IvStatus::NoSolution);

    // A call can never be worth more than the stock itself.
    const Contract call{OptionType::Call, ExerciseStyle::American, 100.0, 0.5};
    EXPECT_EQ(implied_volatility(tree, call, m, 101.0).status, IvStatus::NoSolution);
}
