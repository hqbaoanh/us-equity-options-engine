// Prints how the tree's error against Black-Scholes shrinks with the number
// of steps, and the American put prices for the standard benchmark case.

#include <cmath>
#include <cstdio>

#include "options/binomial.hpp"
#include "options/black_scholes.hpp"

using namespace opt;

int main() {
    const Market m{100.0, 0.05, 0.0, 0.20};
    const Contract call{OptionType::Call, ExerciseStyle::European, 100.0, 1.0};
    const double exact = black_scholes_price(OptionType::Call, m, 100.0, 1.0);

    std::printf("European ATM call (S=K=100, r=5%%, sigma=20%%, T=1), Black-Scholes = %.6f\n\n", exact);
    std::printf("%7s %12s %12s\n", "steps", "tree", "abs error");
    for (int steps = 25; steps <= 3200; steps *= 2) {
        const double tree = BinomialPricer(steps).price(call, m);
        std::printf("%7d %12.6f %12.2e\n", steps, tree, std::abs(tree - exact));
    }

    std::printf("\nAmerican put (K=40, r=6%%, sigma=20%%, T=1), 2000 steps\n\n");
    std::printf("%6s %10s %12s\n", "spot", "tree", "reference");
    const struct { double spot, reference; } cases[] = {{36.0, 4.486}, {40.0, 2.319}, {44.0, 1.113}};
    for (const auto& tc : cases) {
        const Market pm{tc.spot, 0.06, 0.0, 0.20};
        const Contract put{OptionType::Put, ExerciseStyle::American, 40.0, 1.0};
        std::printf("%6.0f %10.4f %12.3f\n", tc.spot, BinomialPricer(2000).price(put, pm), tc.reference);
    }
    return 0;
}
