#include <gtest/gtest.h>

#include "options/binomial.hpp"

using namespace opt;

namespace {

Contract american(OptionType type, double strike, double expiry) {
    return {type, ExerciseStyle::American, strike, expiry};
}

Contract european(OptionType type, double strike, double expiry) {
    return {type, ExerciseStyle::European, strike, expiry};
}

}  // namespace

TEST(BinomialAmerican, MatchesPublishedPutValues) {
    // The standard Longstaff-Schwartz test case (K=40, r=6%, sigma=20%, T=1),
    // checked against finite-difference reference prices from the literature.
    BinomialPricer tree(2000);
    Market m{0.0, 0.06, 0.0, 0.20};
    const struct { double spot, expected; } cases[] = {
        {36.0, 4.486},
        {40.0, 2.319},
        {44.0, 1.113},
    };
    for (const auto& tc : cases) {
        m.spot = tc.spot;
        EXPECT_NEAR(tree.price(american(OptionType::Put, 40.0, 1.0), m), tc.expected, 2e-3)
            << "spot " << tc.spot;
    }
}

TEST(BinomialAmerican, CallWithoutDividendsEqualsEuropean) {
    // Exercising a call early gives up the time value and the interest on K,
    // so with no dividends it is never optimal and the prices must match.
    BinomialPricer tree(500);
    Market m{100.0, 0.05, 0.0, 0.30};
    for (double strike : {60.0, 100.0, 140.0}) {
        EXPECT_NEAR(tree.price(american(OptionType::Call, strike, 1.0), m),
                    tree.price(european(OptionType::Call, strike, 1.0), m), 1e-10)
            << "strike " << strike;
    }
}

TEST(BinomialAmerican, CallWithDividendsCanBeWorthMore) {
    // A high dividend yield makes holding the stock attractive, so a deep
    // in-the-money call is worth exercising early to collect it.
    BinomialPricer tree(500);
    Market m{100.0, 0.02, 0.08, 0.20};
    EXPECT_GT(tree.price(american(OptionType::Call, 60.0, 2.0), m),
              tree.price(european(OptionType::Call, 60.0, 2.0), m) + 0.1);
}

TEST(BinomialAmerican, PutIsWorthAtLeastEuropeanAndIntrinsic) {
    BinomialPricer tree(500);
    Market m{100.0, 0.05, 0.0, 0.25};
    for (double strike : {80.0, 100.0, 130.0, 200.0}) {
        const double am = tree.price(american(OptionType::Put, strike, 1.0), m);
        const double eu = tree.price(european(OptionType::Put, strike, 1.0), m);
        EXPECT_GE(am, eu) << "strike " << strike;
        EXPECT_GE(am, intrinsic_value(OptionType::Put, m.spot, strike)) << "strike " << strike;
    }
}

TEST(BinomialAmerican, DeepInTheMoneyPutIsExercisedImmediately) {
    // At K=200, S=100 the put is so deep in the money that taking the 100
    // now and earning interest on it beats waiting.
    BinomialPricer tree(500);
    Market m{100.0, 0.05, 0.0, 0.25};
    EXPECT_DOUBLE_EQ(tree.price(american(OptionType::Put, 200.0, 1.0), m), 100.0);
}
