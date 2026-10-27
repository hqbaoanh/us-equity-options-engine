#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "options/binomial.hpp"
#include "options/black_scholes.hpp"

using namespace opt;

namespace {

double tree_error(int steps, OptionType type, double strike) {
    Market m{100.0, 0.05, 0.01, 0.25};
    Contract c{type, ExerciseStyle::European, strike, 1.0};
    const double tree = BinomialPricer(steps).price(c, m);
    const double exact = black_scholes_price(type, m, strike, 1.0);
    return std::abs(tree - exact);
}

}  // namespace

TEST(BinomialEuropean, ConvergesToBlackScholes) {
    for (double strike : {80.0, 100.0, 120.0}) {
        for (OptionType type : {OptionType::Call, OptionType::Put}) {
            EXPECT_LT(tree_error(2000, type, strike), 5e-3) << "strike " << strike;
        }
    }
}

TEST(BinomialEuropean, ErrorShrinksWithMoreSteps) {
    // CRR error oscillates with the step count, so compare far-apart sizes
    // rather than expecting it to drop at every step.
    for (double strike : {80.0, 100.0, 120.0}) {
        EXPECT_LT(tree_error(1600, OptionType::Call, strike),
                  tree_error(50, OptionType::Call, strike))
            << "strike " << strike;
    }
}

TEST(BinomialEuropean, SatisfiesPutCallParity) {
    // The tree uses the same risk-neutral drift as the continuous model,
    // so parity holds exactly, not just in the limit.
    Market m{100.0, 0.05, 0.02, 0.30};
    BinomialPricer tree(200);
    for (double strike : {70.0, 100.0, 140.0}) {
        Contract call{OptionType::Call, ExerciseStyle::European, strike, 0.5};
        Contract put{OptionType::Put, ExerciseStyle::European, strike, 0.5};
        const double lhs = tree.price(call, m) - tree.price(put, m);
        const double rhs = m.spot * std::exp(-m.dividend_yield * 0.5) - strike * std::exp(-m.rate * 0.5);
        EXPECT_NEAR(lhs, rhs, 1e-9) << "strike " << strike;
    }
}

TEST(BinomialEuropean, ZeroExpiryIsIntrinsic) {
    Market m{100.0, 0.05, 0.0, 0.2};
    Contract c{OptionType::Put, ExerciseStyle::European, 110.0, 0.0};
    EXPECT_DOUBLE_EQ(BinomialPricer(100).price(c, m), 10.0);
}

TEST(BinomialEuropean, RejectsBadInputs) {
    EXPECT_THROW(BinomialPricer(0), std::invalid_argument);

    BinomialPricer tree(100);
    Contract c{OptionType::Call, ExerciseStyle::European, 100.0, 1.0};
    EXPECT_THROW(tree.price(c, Market{-1.0, 0.05, 0.0, 0.2}), std::invalid_argument);
    EXPECT_THROW(tree.price(c, Market{100.0, 0.05, 0.0, 0.0}), std::invalid_argument);

    Contract negative_expiry{OptionType::Call, ExerciseStyle::European, 100.0, -1.0};
    EXPECT_THROW(tree.price(negative_expiry, Market{100.0, 0.05, 0.0, 0.2}), std::invalid_argument);
}

TEST(BinomialEuropean, RejectsTooFewStepsForTheRate) {
    // With one step over 10 years at 50% rates, e^{r dt} > u and p > 1.
    Market m{100.0, 0.50, 0.0, 0.10};
    Contract c{OptionType::Call, ExerciseStyle::European, 100.0, 10.0};
    EXPECT_THROW(BinomialPricer(1).price(c, m), std::domain_error);
}
