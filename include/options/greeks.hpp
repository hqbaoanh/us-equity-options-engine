#pragma once

namespace opt {

// Sensitivities of the option price.
// theta is per year and vega per 1.0 of volatility, so divide by 365 for
// per-day theta and by 100 for per-vol-point vega.
struct Greeks {
    double delta;
    double gamma;
    double theta;
    double vega;
};

}  // namespace opt
