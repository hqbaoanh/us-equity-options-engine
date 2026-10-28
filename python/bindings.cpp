#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "options/batch.hpp"
#include "options/binomial.hpp"
#include "options/black_scholes.hpp"
#include "options/implied_vol.hpp"

namespace py = pybind11;
using namespace opt;

namespace {

// Column-oriented entry point so a pandas DataFrame can be passed straight
// in. Returns NaN wherever the solver didn't converge.
std::vector<double> implied_vol_chain(const Pricer& pricer, const std::vector<bool>& is_call,
                                      const std::vector<double>& strikes,
                                      const std::vector<double>& expiries,
                                      const std::vector<double>& prices, const Market& market,
                                      bool american, unsigned threads) {
    const std::size_t n = strikes.size();
    if (is_call.size() != n || expiries.size() != n || prices.size() != n) {
        throw std::invalid_argument("all columns must have the same length");
    }

    const ExerciseStyle style = american ? ExerciseStyle::American : ExerciseStyle::European;
    std::vector<Quote> quotes;
    quotes.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const OptionType type = is_call[i] ? OptionType::Call : OptionType::Put;
        quotes.push_back({{type, style, strikes[i], expiries[i]}, prices[i]});
    }

    const std::vector<IvResult> results = implied_vol_batch(pricer, quotes, market, threads);

    std::vector<double> vols(n);
    for (std::size_t i = 0; i < n; ++i) {
        vols[i] = results[i].status == IvStatus::Converged ? results[i].volatility
                                                           : std::numeric_limits<double>::quiet_NaN();
    }
    return vols;
}

}  // namespace

PYBIND11_MODULE(options_engine, m) {
    m.doc() = "American option pricing, Greeks and implied volatility";

    py::enum_<OptionType>(m, "OptionType")
        .value("Call", OptionType::Call)
        .value("Put", OptionType::Put);

    py::enum_<ExerciseStyle>(m, "ExerciseStyle")
        .value("European", ExerciseStyle::European)
        .value("American", ExerciseStyle::American);

    py::enum_<IvStatus>(m, "IvStatus")
        .value("Converged", IvStatus::Converged)
        .value("NoSolution", IvStatus::NoSolution)
        .value("Undetermined", IvStatus::Undetermined)
        .value("NotConverged", IvStatus::NotConverged);

    py::class_<Contract>(m, "Contract")
        .def(py::init<OptionType, ExerciseStyle, double, double>(), py::arg("type"), py::arg("style"),
             py::arg("strike"), py::arg("expiry"))
        .def_readwrite("type", &Contract::type)
        .def_readwrite("style", &Contract::style)
        .def_readwrite("strike", &Contract::strike)
        .def_readwrite("expiry", &Contract::expiry);

    py::class_<Market>(m, "Market")
        .def(py::init<double, double, double, double>(), py::arg("spot"), py::arg("rate"),
             py::arg("dividend_yield") = 0.0, py::arg("volatility") = 0.2)
        .def_readwrite("spot", &Market::spot)
        .def_readwrite("rate", &Market::rate)
        .def_readwrite("dividend_yield", &Market::dividend_yield)
        .def_readwrite("volatility", &Market::volatility);

    py::class_<Greeks>(m, "Greeks")
        .def_readonly("delta", &Greeks::delta)
        .def_readonly("gamma", &Greeks::gamma)
        .def_readonly("theta", &Greeks::theta)
        .def_readonly("vega", &Greeks::vega)
        .def("__repr__", [](const Greeks& g) {
            return py::str("Greeks(delta={:.4f}, gamma={:.4f}, theta={:.4f}, vega={:.4f})")
                .format(g.delta, g.gamma, g.theta, g.vega);
        });

    py::class_<IvResult>(m, "IvResult")
        .def_readonly("volatility", &IvResult::volatility)
        .def_readonly("iterations", &IvResult::iterations)
        .def_readonly("status", &IvResult::status);

    py::class_<Pricer>(m, "Pricer").def("price", &Pricer::price, py::arg("contract"), py::arg("market"));

    py::class_<BinomialPricer, Pricer>(m, "BinomialPricer")
        .def(py::init<int>(), py::arg("steps") = 500)
        .def_property_readonly("steps", &BinomialPricer::steps)
        .def("greeks", &BinomialPricer::greeks, py::arg("contract"), py::arg("market"));

    py::class_<BlackScholesPricer, Pricer>(m, "BlackScholesPricer").def(py::init<>());

    m.def("implied_volatility",
          [](const Pricer& pricer, const Contract& contract, const Market& market, double price) {
              return implied_volatility(pricer, contract, market, price);
          },
          py::arg("pricer"), py::arg("contract"), py::arg("market"), py::arg("price"));

    // The batch runs entirely in C++, so release the GIL and let Python
    // threads keep working while it runs.
    m.def("implied_vol_chain", &implied_vol_chain, py::arg("pricer"), py::arg("is_call"),
          py::arg("strikes"), py::arg("expiries"), py::arg("prices"), py::arg("market"),
          py::arg("american") = true, py::arg("threads") = 0u,
          py::call_guard<py::gil_scoped_release>());
}
