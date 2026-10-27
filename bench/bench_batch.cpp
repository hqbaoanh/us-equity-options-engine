// Times batch pricing and IV solving on a synthetic 10,000-contract chain
// across thread counts.
//
//   ./bench_batch [steps] [max_threads]

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "options/batch.hpp"
#include "options/binomial.hpp"

using namespace opt;

namespace {

// 50 expiries x 100 strikes x calls and puts = 10,000 American contracts.
std::vector<Contract> make_chain() {
    std::vector<Contract> chain;
    for (int e = 1; e <= 50; ++e) {
        const double expiry = e / 50.0;
        for (int k = 0; k < 100; ++k) {
            const double strike = 50.0 + k;
            chain.push_back({OptionType::Call, ExerciseStyle::American, strike, expiry});
            chain.push_back({OptionType::Put, ExerciseStyle::American, strike, expiry});
        }
    }
    return chain;
}

template <typename F>
double best_of_3_ms(F run) {
    double best = 1e300;
    for (int rep = 0; rep < 3; ++rep) {
        const auto start = std::chrono::steady_clock::now();
        run();
        const auto stop = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::milli>(stop - start).count());
    }
    return best;
}

}  // namespace

int main(int argc, char** argv) {
    const int steps = argc > 1 ? std::atoi(argv[1]) : 500;
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const unsigned max_threads = argc > 2 ? static_cast<unsigned>(std::atoi(argv[2])) : hw;

    const BinomialPricer tree(steps);
    Market m{100.0, 0.05, 0.01, 0.3};
    const auto chain = make_chain();

    // Quotes for the IV benchmark: price each contract on a simple smile.
    std::vector<Quote> quotes;
    quotes.reserve(chain.size());
    for (const auto& c : chain) {
        Market smile = m;
        smile.volatility = 0.2 + 0.003 * std::abs(c.strike - m.spot);
        quotes.push_back({c, tree.price(c, smile)});
    }

    std::vector<unsigned> thread_counts;
    for (unsigned t = 1; t < max_threads; t *= 2) {
        thread_counts.push_back(t);
    }
    thread_counts.push_back(max_threads);

    std::printf("%zu American contracts, %d tree steps, %u hardware threads\n\n", chain.size(), steps, hw);
    std::printf("%8s %14s %9s %14s %9s\n", "threads", "price (ms)", "speedup", "IV (ms)", "speedup");

    double price_base = 0.0;
    double iv_base = 0.0;
    for (unsigned t : thread_counts) {
        const double price_ms = best_of_3_ms([&] { price_batch(tree, chain, m, t); });
        const double iv_ms = best_of_3_ms([&] { implied_vol_batch(tree, quotes, m, t); });
        if (t == 1) {
            price_base = price_ms;
            iv_base = iv_ms;
        }
        std::printf("%8u %14.1f %8.2fx %14.1f %8.2fx\n", t, price_ms, price_base / price_ms, iv_ms,
                    iv_base / iv_ms);
    }
    return 0;
}
