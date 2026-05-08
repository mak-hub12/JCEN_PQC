#!/usr/bin/env python3
"""
Harmonize raw CSVs from two experimental datasets into a single canonical schema.

Dataset 1 (2026_SATC_PQC): Dilithium2, Kyber512, P256 — 400 runs, truncated to 370.
Dataset 2 (2026_SATC_PQC_2): Dilithium2 (run2), Falcon512, SPHINCS+ — already 370 runs.

Canonical PPK column names follow Dataset 2 (ppk2_* prefix).
Duration is always ppk2_duration_ms (milliseconds).
Provenance is tracked via 'dataset' and 'run_index' columns.

Outputs one CSV per algorithm to data/processed/.
"""

import pandas as pd
import numpy as np
from pathlib import Path

RUNS_TARGET = 370
OPS_PER_RUN = 3  # keygen, sign/encaps, verify/decaps
ROWS_TARGET = RUNS_TARGET * OPS_PER_RUN  # 1110

RAW_DIR = Path(__file__).parent.parent / "data" / "raw"
OUT_DIR = Path(__file__).parent.parent / "data" / "processed"
OUT_DIR.mkdir(exist_ok=True)

# Canonical column order for all output files
CANONICAL_COLS = [
    "dataset", "run_index",
    "board", "mcu", "freq_mhz", "timestamp_ms",
    "alg", "op", "bytes_in", "bytes_out",
    "cycles", "usec", "energy_uJ",
    "stack_hwm_words", "heap_before", "heap_after",
    "toolchain", "Oflag", "lto", "lib_commit", "fw_git",
    "tmp117_cC", "sht31_cC", "sht31_rh_dpermil",
    "ppk2_start_ms", "ppk2_end_ms", "ppk2_duration_ms",
    "ppk2_samples", "ppk2_current_mean_uA",
    "ppk2_current_p95_uA", "ppk2_current_max_uA",
    "ppk2_charge_uC", "ppk2_energy_uJ",
]

# Dataset 1 Dilithium/Kyber: ppk_ → ppk2_ rename map
PPK_RENAME = {
    "ppk_start_ms":       "ppk2_start_ms",
    "ppk_end_ms":         "ppk2_end_ms",
    "ppk_duration_ms":    "ppk2_duration_ms",
    "ppk_n_samples":      "ppk2_samples",
    "ppk_current_mean_uA": "ppk2_current_mean_uA",
    "ppk_current_p95_uA": "ppk2_current_p95_uA",
    "ppk_current_max_uA": "ppk2_current_max_uA",
    "ppk_charge_uC":      "ppk2_charge_uC",
    "ppk_energy_uJ":      "ppk2_energy_uJ",
}


def add_run_index(df):
    """Assign integer run_index (0-based) to each group of OPS_PER_RUN rows."""
    df = df.reset_index(drop=True)
    df["run_index"] = df.index // OPS_PER_RUN
    return df


def normalize_op(df):
    """Standardize operation names."""
    df["op"] = df["op"].replace("verify_ok", "verify")
    return df


def ensure_canonical(df):
    """Add missing canonical columns as NaN, drop unknowns, reorder."""
    for col in CANONICAL_COLS:
        if col not in df.columns:
            df[col] = np.nan
    return df[CANONICAL_COLS]


def load_dataset1_ppk_prefix(path, dataset_label, alg_label):
    """Load a Dataset 1 CSV that uses the ppk_ (not ppk2_) prefix."""
    df = pd.read_csv(path, nrows=ROWS_TARGET)
    df = df.rename(columns=PPK_RENAME)
    df["dataset"] = dataset_label
    df["alg"] = alg_label
    df = normalize_op(df)
    df = add_run_index(df)
    return ensure_canonical(df)


def load_dataset1_p256(path, dataset_label):
    """Load Dataset 1 P256 CSV: already uses ppk2_ prefix but duration is in seconds."""
    df = pd.read_csv(path, nrows=ROWS_TARGET)
    # Convert duration seconds → milliseconds
    df = df.rename(columns={"ppk2_duration_s": "ppk2_duration_ms"})
    df["ppk2_duration_ms"] = df["ppk2_duration_ms"] * 1000.0
    df["dataset"] = dataset_label
    df = normalize_op(df)
    df = add_run_index(df)
    return ensure_canonical(df)


def load_dataset2(path, dataset_label):
    """Load a Dataset 2 CSV: already in canonical ppk2_ schema."""
    df = pd.read_csv(path)
    df["dataset"] = dataset_label
    df = normalize_op(df)
    df = add_run_index(df)
    return ensure_canonical(df)


def validate(df, name):
    n_runs = df["run_index"].nunique()
    n_rows = len(df)
    ops = sorted(df["op"].unique())
    print(f"  {name}: {n_rows} rows, {n_runs} runs, ops={ops}")
    assert n_rows == ROWS_TARGET, f"Expected {ROWS_TARGET} rows, got {n_rows}"
    assert n_runs == RUNS_TARGET, f"Expected {RUNS_TARGET} runs, got {n_runs}"
    assert set(ops) <= {"keygen", "sign", "encaps", "decaps", "verify"}, \
        f"Unexpected ops: {ops}"


def main():
    sources = [
        # (loader, args, output_stem)
        (load_dataset1_ppk_prefix,
         (RAW_DIR / "dataset1_Dilithium2_400.csv", "dataset1", "dilithium2"),
         "dilithium2_dataset1_370"),

        (load_dataset1_ppk_prefix,
         (RAW_DIR / "dataset1_Kyber512_400.csv", "dataset1", "kyber512"),
         "kyber512_dataset1_370"),

        (load_dataset1_p256,
         (RAW_DIR / "dataset1_P256_400.csv", "dataset1"),
         "p256_dataset1_370"),

        (load_dataset2,
         (RAW_DIR / "dataset2_Dilithium2_370.csv", "dataset2"),
         "dilithium2_dataset2_370"),

        (load_dataset2,
         (RAW_DIR / "dataset2_Falcon512_370.csv", "dataset2"),
         "falcon512_dataset2_370"),

        (load_dataset2,
         (RAW_DIR / "dataset2_SPHINCS_370.csv", "dataset2"),
         "sphincs_dataset2_370"),
    ]

    print(f"Harmonizing to {RUNS_TARGET} runs each ({ROWS_TARGET} rows)...\n")

    for loader, args, stem in sources:
        df = loader(*args)
        validate(df, stem)
        out_path = OUT_DIR / f"{stem}.csv"
        df.to_csv(out_path, index=False)
        print(f"  -> {out_path.relative_to(OUT_DIR.parent.parent)}\n")

    print("Done. Processed files:")
    for f in sorted(OUT_DIR.glob("*.csv")):
        print(f"  {f.name}  ({f.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
