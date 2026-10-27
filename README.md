# US Equity Options Pricing Engine

Multithreaded C++ pricer for American-style US equity options, with implied volatility and Greeks.

Work in progress.

## Build and test

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
