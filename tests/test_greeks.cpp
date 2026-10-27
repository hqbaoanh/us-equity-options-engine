#include <gtest/gtest.h>

#include <stdexcept>

#include "options/binomial.hpp"
#include "options/black_scholes.hpp"

using namespace opt;

TEST(BlackScholesGreeks, MatchesFiniteDifferences) {
    // Check the closed forms against bumping the closed-form price, so the
    // reference used below is itself trustworthy.
    Market m{100.0, 0.05, 0.02, 0.25};
    const double k = 105.0;
    const double t = 0.8;
    const double h = 1e-4;
    for (OptionType type : {OptionType::Call, OptionType::Put}) {
        const Greeks g = black_scholes_greeks(type, m, k, t);

        Market s_up = m, s_down = m, v_up = m, v_down = m;
        s_up.spot += h;
        s_down.spot -= h;
        v_up.volatility += h;
        v_down.volatility -= h;
        const double p = black_scholes_price(type, m, k, t);
        const double p_su = black_scholes_price(type, s_up, k, t);
        const double p_sd = black_scholes_price(type, s_down, k, t);

        EXPECT_NEAR(g.delta, (p_su - p_sd) / (2 * h), 1e-6);
        EXPECT_NEAR(g.gamma, (p_su - 2 * p + p_sd) / (h * h), 1e-4);
        EXPECT_NEAR(g.vega,
                    (black_scholes_price(type, v_up, k, t) - black_scholes_price(type, v_down, k, t)) / (2 * h),
                    1e-5);
        // Theta is the change in value as calendar time passes, i.e. as expiry shrinks.
        EXPECT_NEAR(g.theta,
                    (black_scholes_price(type, m, k, t - h) - black_scholes_price(type, m, k, t + h)) / (2 * h),
                    1e-5);
    }
}

TEST(TreeGreeks, MatchBlackScholesForEuropeans) {
    BinomialPricer tree(2000);
    Market m{100.0, 0.05, 0.01, 0.25};
    for (double strike : {85.0, 100.0, 115.0}) {
        for (OptionType type : {OptionType::Call, OptionType::Put}) {
            const Contract c{type, ExerciseStyle::European, strike, 1.0};
            const Greeks tg = tree.greeks(c, m);
            const Greeks bs = black_scholes_greeks(type, m, strike, 1.0);

            SCOPED_TRACE(testing::Message() << "strike " << strike
                                            << (type == OptionType::Call ? " call" : " put"));
            EXPECT_NEAR(tg.delta, bs.delta, 2e-3);
            EXPECT_NEAR(tg.gamma, bs.gamma, 2e-4);
            EXPECT_NEAR(tg.theta, bs.theta, 2e-2);
            // Bumping volatility shifts every node, so tree vega picks up some of the
            // tree's discretization noise. Hold it to 0.5% rather than an absolute bound.
            EXPECT_NEAR(tg.vega, bs.vega, 5e-3 * bs.vega);
        }
    }
}

TEST(TreeGreeks, AmericanPutHasSensibleSigns) {
    BinomialPricer tree(1000);
    Market m{100.0, 0.05, 0.0, 0.30};
    for (double strike : {80.0, 100.0, 120.0}) {
        const Greeks g = tree.greeks({OptionType::Put, ExerciseStyle::American, strike, 1.0}, m);
        EXPECT_GE(g.delta, -1.0) << "strike " << strike;
        EXPECT_LE(g.delta, 0.0) << "strike " << strike;
        EXPECT_GE(g.gamma, 0.0) << "strike " << strike;
        EXPECT_GT(g.vega, 0.0) << "strike " << strike;
    }
}

TEST(TreeGreeks, ExercisedPutBehavesLikeShortStock) {
    // Deep in the money the put is worth K - S at every early node,
    // so it moves one-for-one against the stock and has no curvature.
    BinomialPricer tree(500);
    Market m{100.0, 0.05, 0.0, 0.25};
    const Greeks g = tree.greeks({OptionType::Put, ExerciseStyle::American, 200.0, 1.0}, m);
    EXPECT_NEAR(g.delta, -1.0, 1e-12);
    EXPECT_NEAR(g.gamma, 0.0, 1e-12);
}

TEST(TreeGreeks, WorkWithTheSmallestTreeThatHasTwoLevels) {
    BinomialPricer tree(2);
    const Greeks g = tree.greeks({OptionType::Call, ExerciseStyle::European, 100.0, 1.0},
                                 Market{100.0, 0.05, 0.0, 0.2});
    EXPECT_GT(g.delta, 0.0);
    EXPECT_LT(g.delta, 1.0);
}

TEST(TreeGreeks, RejectInvalidSetups) {
    Market m{100.0, 0.05, 0.0, 0.2};
    EXPECT_THROW(BinomialPricer(1).greeks({OptionType::Call, ExerciseStyle::European, 100.0, 1.0}, m),
                 std::invalid_argument);
    EXPECT_THROW(BinomialPricer(100).greeks({OptionType::Call, ExerciseStyle::European, 100.0, 0.0}, m),
                 std::invalid_argument);
}
