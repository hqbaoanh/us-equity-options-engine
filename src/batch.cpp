#include "options/batch.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <thread>

namespace opt {

namespace {

// Runs body(i) for every i in [0, n) on up to `threads` threads.
//
// Workers pull small chunks from a shared counter instead of each getting a
// fixed slice up front. IV solves take a varying number of iterations, so
// fixed slices would leave some threads idle while others finish their share.
template <typename Body>
void parallel_for(std::size_t n, unsigned threads, Body body) {
    if (threads == 0) {
        threads = std::max(1u, std::thread::hardware_concurrency());
    }
    threads = static_cast<unsigned>(std::min<std::size_t>(threads, n));
    if (threads <= 1) {
        for (std::size_t i = 0; i < n; ++i) {
            body(i);
        }
        return;
    }

    constexpr std::size_t kChunk = 16;
    std::atomic<std::size_t> next{0};
    std::vector<std::exception_ptr> errors(threads);

    auto worker = [&](unsigned id) {
        try {
            for (;;) {
                const std::size_t begin = next.fetch_add(kChunk, std::memory_order_relaxed);
                if (begin >= n) {
                    return;
                }
                const std::size_t end = std::min(begin + kChunk, n);
                for (std::size_t i = begin; i < end; ++i) {
                    body(i);
                }
            }
        } catch (...) {
            // An exception escaping a std::thread would call std::terminate,
            // so hold on to it and rethrow on the calling thread.
            errors[id] = std::current_exception();
            next.store(n, std::memory_order_relaxed);  // tell the others to stop
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads - 1);
    for (unsigned id = 1; id < threads; ++id) {
        pool.emplace_back(worker, id);
    }
    worker(0);  // the calling thread does its share too
    for (auto& t : pool) {
        t.join();
    }

    for (const auto& e : errors) {
        if (e) {
            std::rethrow_exception(e);
        }
    }
}

}  // namespace

std::vector<double> price_batch(const Pricer& pricer, const std::vector<Contract>& contracts,
                                const Market& market, unsigned threads) {
    std::vector<double> prices(contracts.size());
    parallel_for(contracts.size(), threads,
                 [&](std::size_t i) { prices[i] = pricer.price(contracts[i], market); });
    return prices;
}

std::vector<IvResult> implied_vol_batch(const Pricer& pricer, const std::vector<Quote>& quotes,
                                        const Market& market, unsigned threads,
                                        const IvOptions& options) {
    std::vector<IvResult> results(quotes.size());
    parallel_for(quotes.size(), threads, [&](std::size_t i) {
        results[i] = implied_volatility(pricer, quotes[i].contract, market, quotes[i].market_price, options);
    });
    return results;
}

}  // namespace opt
