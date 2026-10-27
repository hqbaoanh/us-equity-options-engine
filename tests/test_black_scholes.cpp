#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "options/black_scholes.hpp"

using namespace opt;

TEST(BlackScholes, MatchesKnownValues) {
    // S=100, K=100, r=5%, sigma=20%, T=1
    Market m{100.0, 0.05, 0.0, 0.20};
    EXPECT_NEAR(black_scholes_price(OptionType::Call, m, 100.0, 1.0), 10.450584, 1e-6);
    EXPECT_NEAR(black_scholes_price(OptionType::Put, m, 100.0, 1.0), 5.573526, 1e-6);

    // Hull's textbook example: S=42, K=40, r=10%, sigma=20%, T=0.5
    Market hull{42.0, 0.10, 0.0, 0.20};
    EXPECT_NEAR(black_scholes_price(OptionType::Call, hull, 40.0, 0.5), 4.7594, 1e-4);
    EXPECT_NEAR(black_scholes_price(OptionType::Put, hull, 40.0, 0.5), 0.8086, 1e-4);
}

TEST(BlackScholes, PutCallParityWithDividends) {
    // C - P = S e^{-qT} - K e^{-rT}
    for (double strike : {60.0, 90.0, 100.0, 120.0, 180.0}) {
        Market m{100.0, 0.04, 0.02, 0.35};
        const double t = 0.75;
        const double call = black_scholes_price(OptionType::Call, m, strike, t);
        const double put = black_scholes_price(OptionType::Put, m, strike, t);
        const double rhs = m.spot * std::exp(-m.dividend_yield * t) - strike * std::exp(-m.rate * t);
        EXPECT_NEAR(call - put, rhs, 1e-10) << "strike " << strike;
    }
}

TEST(BlackScholes, ZeroExpiryIsIntrinsic) {
    Market m{100.0, 0.05, 0.0, 0.20};
    EXPECT_DOUBLE_EQ(black_scholes_price(OptionType::Call, m, 90.0, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(black_scholes_price(OptionType::Put, m, 90.0, 0.0), 0.0);
}

TEST(BlackScholes, PricerRejectsAmerican) {
    BlackScholesPricer bs;
    Contract c{OptionType::Put, ExerciseStyle::American, 100.0, 1.0};
    EXPECT_THROW(bs.price(c, Market{100.0, 0.05, 0.0, 0.2}), std::invalid_argument);
}
