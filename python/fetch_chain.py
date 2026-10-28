"""Download a US equity option chain from Yahoo Finance and save it as CSV.

    python fetch_chain.py AAPL --rate 0.04 --div-yield 0.005

Quotes are only meaningful during market hours; outside them bids and asks
are often zero or stale.
"""

import argparse
from datetime import datetime, timezone
from pathlib import Path

import pandas as pd
import yfinance as yf

SECONDS_PER_YEAR = 365.0 * 24 * 3600


def years_to_expiry(expiration: str, now: pd.Timestamp) -> float:
    # US equity options stop trading at 4pm New York time on expiration day.
    close = pd.Timestamp(f"{expiration} 16:00", tz="America/New_York")
    return (close - now).total_seconds() / SECONDS_PER_YEAR


def spot_price(ticker: yf.Ticker) -> float:
    try:
        return float(ticker.fast_info["last_price"])
    except (KeyError, TypeError):
        return float(ticker.history(period="5d")["Close"].iloc[-1])


def fetch(symbol: str, max_expiries: int) -> pd.DataFrame:
    ticker = yf.Ticker(symbol)
    expirations = ticker.options[:max_expiries]
    if not expirations:
        raise SystemExit(f"no listed options found for {symbol}")

    now = pd.Timestamp(datetime.now(timezone.utc))
    spot = spot_price(ticker)

    frames = []
    for expiration in expirations:
        chain = ticker.option_chain(expiration)
        for side, df in (("call", chain.calls), ("put", chain.puts)):
            frames.append(
                pd.DataFrame(
                    {
                        "type": side,
                        "expiration": expiration,
                        "expiry_years": years_to_expiry(expiration, now),
                        "strike": df["strike"],
                        "bid": df["bid"],
                        "ask": df["ask"],
                        "last": df["lastPrice"],
                        "volume": df["volume"],
                        "open_interest": df["openInterest"],
                    }
                )
            )

    out = pd.concat(frames, ignore_index=True)
    out["spot"] = spot
    out["fetched_at"] = now.isoformat()
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("symbol")
    parser.add_argument("--rate", type=float, required=True,
                        help="continuously compounded risk-free rate, e.g. 0.04 (use a current T-bill yield)")
    parser.add_argument("--div-yield", type=float, default=0.0,
                        help="continuous dividend yield, e.g. 0.005 for 0.5%%")
    parser.add_argument("--max-expiries", type=int, default=8)
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()

    chain = fetch(args.symbol.upper(), args.max_expiries)
    chain["rate"] = args.rate
    chain["div_yield"] = args.div_yield

    out = args.out or Path(__file__).resolve().parent.parent / "data" / f"{args.symbol.upper()}_chain.csv"
    out.parent.mkdir(parents=True, exist_ok=True)
    chain.to_csv(out, index=False)
    print(f"saved {len(chain)} quotes across {chain['expiration'].nunique()} expiries to {out}")


if __name__ == "__main__":
    main()
