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

// Everything about the market a pricer needs besides the contract itself.
// Rates and yields are continuously compounded, annualized.
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
