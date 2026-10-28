import math

import pytest

import options_engine as oe


def test_tree_matches_black_scholes_for_european():
    market = oe.Market(spot=100.0, rate=0.05, dividend_yield=0.0, volatility=0.2)
    call = oe.Contract(oe.OptionType.Call, oe.ExerciseStyle.European, 100.0, 1.0)
    tree = oe.BinomialPricer(2000).price(call, market)
    exact = oe.BlackScholesPricer().price(call, market)
    assert tree == pytest.approx(exact, abs=5e-3)


def test_greeks_are_exposed():
    market = oe.Market(100.0, 0.05, 0.0, 0.3)
    put = oe.Contract(oe.OptionType.Put, oe.ExerciseStyle.American, 100.0, 0.5)
    g = oe.BinomialPricer(500).greeks(put, market)
    assert -1.0 <= g.delta <= 0.0
    assert g.gamma > 0.0
    assert g.vega > 0.0


def test_single_implied_vol_round_trip():
    tree = oe.BinomialPricer(300)
    market = oe.Market(100.0, 0.05, 0.0, 0.42)
    put = oe.Contract(oe.OptionType.Put, oe.ExerciseStyle.American, 95.0, 0.25)
    price = tree.price(put, market)

    result = oe.implied_volatility(tree, put, market, price)
    assert result.status == oe.IvStatus.Converged
    assert result.volatility == pytest.approx(0.42, abs=1e-4)


def test_chain_round_trip_and_nan_for_bad_quotes():
    tree = oe.BinomialPricer(200)
    market = oe.Market(100.0, 0.05, 0.0, 0.0)

    is_call, strikes, expiries, prices, vols = [], [], [], [], []
    for k in (85.0, 95.0, 105.0, 115.0):
        call = k > market.spot
        vol = 0.2 + 0.004 * abs(k - market.spot)
        market.volatility = vol
        option_type = oe.OptionType.Call if call else oe.OptionType.Put
        contract = oe.Contract(option_type, oe.ExerciseStyle.American, k, 0.5)
        is_call.append(call)
        strikes.append(k)
        expiries.append(0.5)
        prices.append(tree.price(contract, market))
        vols.append(vol)

    # A call quoted above the stock price is impossible, so it should come back NaN.
    is_call.append(True)
    strikes.append(100.0)
    expiries.append(0.5)
    prices.append(150.0)

    out = oe.implied_vol_chain(tree, is_call, strikes, expiries, prices, market, american=True, threads=2)
    assert out[:-1] == pytest.approx(vols, abs=1e-4)
    assert math.isnan(out[-1])


def test_chain_rejects_mismatched_columns():
    tree = oe.BinomialPricer(50)
    with pytest.raises(ValueError):
        oe.implied_vol_chain(tree, [True], [100.0, 105.0], [0.5], [1.0], oe.Market(100.0, 0.05))
