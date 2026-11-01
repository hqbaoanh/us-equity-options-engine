#pragma once

namespace opt {

enum class OptionType { Call, Put };
enum class ExerciseStyle { European, American };

struct Contract {
    OptionType type;
    ExerciseStyle style;
    double strike;
    double expiry;  // years
};

// rate and dividend_yield: continuously compounded, annual
struct Market {
    double spot;
    double rate;
    double dividend_yield;
    double volatility;
};

inline double intrinsic_value(OptionType type, double spot, double strike) {
    double payoff = (type == OptionType::Call) ? spot - strike : strike - spot;
    return payoff > 0.0 ? payoff : 0.0;
}

}  // namespace opt
