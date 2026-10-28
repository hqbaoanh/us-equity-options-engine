"""Solve implied volatilities for a saved chain and plot the smile and surface.

    python iv_surface.py ../data/AAPL_chain.csv

Writes iv_smile.png, iv_surface.png and the per-quote IVs to ../results/.
"""

import argparse
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

import options_engine as oe


def clean_quotes(chain: pd.DataFrame, min_days: float) -> pd.DataFrame:
    df = chain.copy()
    df["mid"] = 0.5 * (df["bid"] + df["ask"])
    df["moneyness"] = df["strike"] / df["spot"]

    # Out-of-the-money options only: they are the liquid side of each strike,
    # and their prices are almost all time value, which is what IV measures.
    otm = ((df["type"] == "call") & (df["strike"] > df["spot"])) | (
        (df["type"] == "put") & (df["strike"] < df["spot"])
    )
    two_sided = (df["bid"] > 0) & (df["ask"] > df["bid"])
    tight = (df["ask"] - df["bid"]) / df["mid"] <= 0.5
    not_too_short = df["expiry_years"] * 365 >= min_days
    near_money = df["moneyness"].between(0.7, 1.3)

    return df[otm & two_sided & tight & not_too_short & near_money].reset_index(drop=True)


def plot_smiles(df: pd.DataFrame, symbol: str, out: Path) -> None:
    fig, ax = plt.subplots(figsize=(8, 5))
    for expiration, group in df.groupby("expiration"):
        group = group.sort_values("moneyness")
        days = group["expiry_years"].iloc[0] * 365
        ax.plot(group["moneyness"], group["iv"] * 100, marker=".", label=f"{expiration} ({days:.0f}d)")
    ax.axvline(1.0, color="grey", linewidth=0.8, linestyle="--")
    ax.set_xlabel("Moneyness K / S")
    ax.set_ylabel("Implied volatility (%)")
    ax.set_title(f"{symbol} implied volatility smile")
    ax.legend(fontsize=8)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def plot_surface(df: pd.DataFrame, symbol: str, out: Path) -> None:
    fig = plt.figure(figsize=(9, 6))
    ax = fig.add_subplot(projection="3d")
    ax.plot_trisurf(df["moneyness"], df["expiry_years"] * 365, df["iv"] * 100, cmap="viridis",
                    linewidth=0.1, antialiased=True)
    ax.set_xlabel("Moneyness K / S")
    ax.set_ylabel("Days to expiry")
    ax.set_zlabel("IV (%)")
    ax.set_title(f"{symbol} implied volatility surface")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("chain_csv", type=Path)
    parser.add_argument("--steps", type=int, default=300)
    parser.add_argument("--threads", type=int, default=0, help="0 uses every core")
    parser.add_argument("--min-days", type=float, default=7.0)
    parser.add_argument("--out-dir", type=Path, default=Path(__file__).resolve().parent.parent / "results")
    args = parser.parse_args()

    chain = pd.read_csv(args.chain_csv)
    symbol = args.chain_csv.stem.split("_")[0]
    df = clean_quotes(chain, args.min_days)
    if df.empty:
        raise SystemExit("no usable quotes left after filtering (was the chain fetched during market hours?)")

    first = df.iloc[0]
    market = oe.Market(spot=first["spot"], rate=first["rate"], dividend_yield=first["div_yield"])
    tree = oe.BinomialPricer(args.steps)

    start = time.perf_counter()
    df["iv"] = oe.implied_vol_chain(
        tree,
        (df["type"] == "call").tolist(),
        df["strike"].tolist(),
        df["expiry_years"].tolist(),
        df["mid"].tolist(),
        market,
        american=True,
        threads=args.threads,
    )
    elapsed = time.perf_counter() - start

    solved = df.dropna(subset=["iv"])
    print(f"{symbol}: solved {len(solved)}/{len(df)} quotes in {elapsed * 1000:.0f} ms "
          f"({args.steps}-step tree, spot {market.spot:.2f})")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    solved.to_csv(args.out_dir / f"{symbol}_iv.csv", index=False)
    plot_smiles(solved, symbol, args.out_dir / "iv_smile.png")
    if solved["expiration"].nunique() >= 2:
        plot_surface(solved, symbol, args.out_dir / "iv_surface.png")
    print(f"wrote plots to {args.out_dir}")


if __name__ == "__main__":
    main()
