#!/usr/bin/env python3
"""
Unified analysis across all 6 processed datasets for the JCEN paper.

Algorithms covered:
  Signatures : Dilithium2 (run1+run2), Falcon-512, SPHINCS+-SHA2-128s, P-256 (ECDSA)
  KEM        : Kyber-512 (ML-KEM-512)

Generates:
  figures/latency_comparison.{png,eps}
  figures/energy_comparison.{png,eps}
  figures/variance_cv.{png,eps}
  figures/sign_latency_cdf.{png,eps}
  figures/dilithium2_crossrun.{png,eps}   -- reproducibility check
  figures/summary_table.csv
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from matplotlib.patches import Patch
import numpy as np
import seaborn as sns
from pathlib import Path

PROCESSED_DIR = Path(__file__).parent.parent / "data" / "processed"
FIG_DIR = Path(__file__).parent.parent / "figures"
FIG_DIR.mkdir(exist_ok=True)

# Publication style (JCEN: 84 mm double-col, 174 mm single-col)
MM_TO_IN = 1 / 25.4
COL1_W = 174 * MM_TO_IN   # single-column width in inches
COL2_W = 84 * MM_TO_IN    # double-column width in inches

sns.set_style("whitegrid")
plt.rcParams.update({
    "font.family": "serif",
    "font.size": 9,
    "axes.titlesize": 10,
    "axes.labelsize": 9,
    "xtick.labelsize": 8,
    "ytick.labelsize": 8,
    "legend.fontsize": 8,
    "figure.dpi": 150,
    "savefig.dpi": 300,
    "savefig.bbox": "tight",
})

ALG_DISPLAY = {
    "dilithium2_dataset1": "Dilithium-2\n(run 1)",
    "dilithium2_dataset2": "Dilithium-2\n(run 2)",
    "falcon512":           "Falcon-512",
    "sphincs":             "SPHINCS+\n128s",
    "p256":                "P-256\n(ECDSA)",
    "kyber512":            "Kyber-512",
}
ALG_COLOR = {
    "dilithium2_dataset1": "#1f77b4",
    "dilithium2_dataset2": "#aec7e8",
    "falcon512":           "#ff7f0e",
    "sphincs":             "#2ca02c",
    "p256":                "#d62728",
    "kyber512":            "#9467bd",
}


def load_all():
    files = {
        "dilithium2_dataset1": "dilithium2_dataset1_370.csv",
        "dilithium2_dataset2": "dilithium2_dataset2_370.csv",
        "falcon512":           "falcon512_dataset2_370.csv",
        "sphincs":             "sphincs_dataset2_370.csv",
        "p256":                "p256_dataset1_370.csv",
        "kyber512":            "kyber512_dataset1_370.csv",
    }
    frames = {}
    for key, fname in files.items():
        path = PROCESSED_DIR / fname
        if not path.exists():
            print(f"  WARNING: {fname} not found — run 01_harmonize_data.py first")
            continue
        df = pd.read_csv(path)
        df["alg_key"] = key
        df["latency_ms"] = df["usec"] / 1000
        df["energy_mJ"]  = df["ppk2_energy_uJ"] / 1000
        df["power_mW"]   = (df["ppk2_current_mean_uA"] / 1000) * 3.3
        df["op"] = df["op"].replace("verify_ok", "verify")
        frames[key] = df
        print(f"  Loaded {key}: {len(df)} rows")
    return frames


def summary_stats(frames):
    records = []
    for key, df in frames.items():
        for op in sorted(df["op"].unique()):
            sub = df[df["op"] == op]
            n = len(sub)
            lat = sub["latency_ms"]
            eng = sub["energy_mJ"]
            pwr = sub["power_mW"]
            cv  = lat.std() / lat.mean() * 100 if lat.mean() > 0 else np.nan
            records.append({
                "Algorithm":    ALG_DISPLAY[key].replace("\n", " "),
                "Operation":    op,
                "n":            n,
                "Latency mean (ms)": f"{lat.mean():.2f}",
                "Latency std (ms)":  f"{lat.std():.2f}",
                "Latency CV (%)":    f"{cv:.2f}",
                "Energy mean (mJ)":  f"{eng.mean():.3f}",
                "Energy std (mJ)":   f"{eng.std():.3f}",
                "Power mean (mW)":   f"{pwr.mean():.1f}",
            })
    df_out = pd.DataFrame(records)
    out = FIG_DIR / "summary_table.csv"
    df_out.to_csv(out, index=False)
    print(f"\nSummary table -> {out.name}")
    print(df_out.to_string(index=False))
    return df_out


def _savefig(name):
    for ext in ("png", "eps"):
        plt.savefig(FIG_DIR / f"{name}.{ext}")
    print(f"  -> figures/{name}.{{png,eps}}")
    plt.close()


def plot_latency(frames, sig_keys, kem_keys):
    ops = ["keygen", "sign", "verify"]
    keys = sig_keys
    n = len(keys)
    x = np.arange(len(ops))
    w = 0.8 / n

    fig, ax = plt.subplots(figsize=(COL1_W, COL1_W * 0.55))
    for i, key in enumerate(keys):
        df = frames[key]
        means, stds = [], []
        for op in ops:
            sub = df[df["op"] == op]["latency_ms"]
            means.append(sub.mean() if len(sub) else 0)
            stds.append(sub.std()  if len(sub) else 0)
        offset = w * (i - (n - 1) / 2)
        bars = ax.bar(x + offset, means, w, yerr=stds, capsize=3,
                      label=ALG_DISPLAY[key].replace("\n", " "),
                      color=ALG_COLOR[key], alpha=0.85, edgecolor="black", linewidth=0.5)

    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(ticker.ScalarFormatter())
    ax.set_xticks(x)
    ax.set_xticklabels(["Key Gen", "Sign", "Verify"])
    ax.set_ylabel("Latency (ms)")
    ax.set_title("Signature Latency Comparison (370 runs, mean ± 1σ)")
    ax.legend(loc="upper left", ncol=2)
    ax.grid(True, which="both", alpha=0.3, axis="y")
    _savefig("latency_comparison")


def plot_energy(frames, sig_keys):
    ops = ["keygen", "sign", "verify"]
    keys = sig_keys
    n = len(keys)
    x = np.arange(len(ops))
    w = 0.8 / n

    fig, ax = plt.subplots(figsize=(COL1_W, COL1_W * 0.55))
    for i, key in enumerate(keys):
        df = frames[key]
        means, stds = [], []
        for op in ops:
            sub = df[df["op"] == op]["energy_mJ"]
            means.append(sub.mean() if len(sub) else 0)
            stds.append(sub.std()  if len(sub) else 0)
        offset = w * (i - (n - 1) / 2)
        ax.bar(x + offset, means, w, yerr=stds, capsize=3,
               label=ALG_DISPLAY[key].replace("\n", " "),
               color=ALG_COLOR[key], alpha=0.85, edgecolor="black", linewidth=0.5)

    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(ticker.ScalarFormatter())
    ax.set_xticks(x)
    ax.set_xticklabels(["Key Gen", "Sign", "Verify"])
    ax.set_ylabel("Energy (mJ)")
    ax.set_title("Energy Consumption Comparison (370 runs, mean ± 1σ)")
    ax.legend(loc="upper left", ncol=2)
    ax.grid(True, which="both", alpha=0.3, axis="y")
    _savefig("energy_comparison")


def plot_cv(frames, sig_keys):
    ops = ["keygen", "sign", "verify"]
    keys = sig_keys
    n = len(keys)
    x = np.arange(len(ops))
    w = 0.8 / n

    fig, ax = plt.subplots(figsize=(COL1_W, COL1_W * 0.55))
    for i, key in enumerate(keys):
        df = frames[key]
        cvs = []
        for op in ops:
            sub = df[df["op"] == op]["latency_ms"]
            cvs.append((sub.std() / sub.mean() * 100) if len(sub) and sub.mean() > 0 else 0)
        offset = w * (i - (n - 1) / 2)
        ax.bar(x + offset, cvs, w,
               label=ALG_DISPLAY[key].replace("\n", " "),
               color=ALG_COLOR[key], alpha=0.85, edgecolor="black", linewidth=0.5)

    ax.axhline(1.0, color="red", linestyle="--", linewidth=0.8, alpha=0.6, label="1% reference")
    ax.set_xticks(x)
    ax.set_xticklabels(["Key Gen", "Sign", "Verify"])
    ax.set_ylabel("Coefficient of Variation (%)")
    ax.set_title("Latency Variability: Coefficient of Variation")
    ax.legend(loc="upper right", ncol=2)
    ax.grid(True, alpha=0.3, axis="y")
    _savefig("variance_cv")


def plot_sign_cdf(frames, sig_keys):
    fig, ax = plt.subplots(figsize=(COL2_W * 1.5, COL2_W * 1.1))
    for key in sig_keys:
        df = frames[key]
        sign_col = "sign" if "sign" in df["op"].values else None
        if sign_col is None:
            continue
        data = sorted(df[df["op"] == sign_col]["latency_ms"])
        cdf = np.arange(1, len(data) + 1) / len(data)
        cv = np.std(data) / np.mean(data) * 100
        ax.plot(data, cdf, label=f"{ALG_DISPLAY[key].replace(chr(10), ' ')} (CV={cv:.1f}%)",
                color=ALG_COLOR[key], linewidth=1.5)

    ax.set_xscale("log")
    ax.set_xlabel("Signing Latency (ms)")
    ax.set_ylabel("Cumulative Probability")
    ax.set_title("Signing Latency CDF — Variance Comparison")
    ax.legend(loc="lower right")
    ax.grid(True, alpha=0.3, which="both")
    _savefig("sign_latency_cdf")


def plot_dilithium_crossrun(frames):
    """Reproduce cross-run consistency check for Dilithium2."""
    if "dilithium2_dataset1" not in frames or "dilithium2_dataset2" not in frames:
        print("  Skipping cross-run plot (missing one or both Dilithium2 datasets)")
        return

    ops = ["keygen", "sign", "verify"]
    fig, axes = plt.subplots(1, 3, figsize=(COL1_W, COL1_W * 0.4), sharey=False)

    for ax, op in zip(axes, ops):
        d1 = frames["dilithium2_dataset1"]
        d2 = frames["dilithium2_dataset2"]
        data1 = d1[d1["op"] == op]["latency_ms"]
        data2 = d2[d2["op"] == op]["latency_ms"]

        ax.hist(data1, bins=30, alpha=0.6, color=ALG_COLOR["dilithium2_dataset1"],
                label="Run 1", density=True, edgecolor="none")
        ax.hist(data2, bins=30, alpha=0.6, color=ALG_COLOR["dilithium2_dataset2"],
                label="Run 2", density=True, edgecolor="none")
        ax.set_title(op.capitalize())
        ax.set_xlabel("Latency (ms)")
        if ax == axes[0]:
            ax.set_ylabel("Density")
            ax.legend(fontsize=7)

    fig.suptitle("Dilithium-2 Cross-Run Reproducibility (n=370 each)", fontsize=9)
    plt.tight_layout()
    _savefig("dilithium2_crossrun")


def main():
    print("Loading processed data...\n")
    frames = load_all()

    if not frames:
        print("No data found. Run 01_harmonize_data.py first.")
        return

    sig_keys = [k for k in
                ["dilithium2_dataset1", "dilithium2_dataset2", "falcon512", "sphincs", "p256"]
                if k in frames]
    kem_keys = [k for k in ["kyber512"] if k in frames]

    print("\nGenerating summary table...")
    summary_stats(frames)

    print("\nGenerating figures...")
    plot_latency(frames, sig_keys, kem_keys)
    plot_energy(frames, sig_keys)
    plot_cv(frames, sig_keys)
    plot_sign_cdf(frames, sig_keys)
    plot_dilithium_crossrun(frames)

    if kem_keys:
        print("\n  (Kyber-512 KEM figures: add dedicated KEM plot here)")

    print(f"\nAll outputs in figures/")


if __name__ == "__main__":
    main()
