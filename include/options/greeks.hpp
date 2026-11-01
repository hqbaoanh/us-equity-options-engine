#pragma once

namespace opt {

// theta per year, vega per 1.0 of vol (/365 for per-day theta, /100 for per-vol-point vega)
struct Greeks {
    double delta;
    double gamma;
    double theta;
    double vega;
};

}  // namespace opt
