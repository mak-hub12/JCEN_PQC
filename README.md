# JCEN_PQC — PQC Benchmark on STM32F446RE (ARM Cortex-M4)

Reproducibility artefacts for:

> Austin McGuire. *Comprehensive Performance and Variance Benchmarking of NIST Post-Quantum Cryptographic Standards on ARM Cortex-M4 Embedded Hardware*. Submitted to Journal of Cryptographic Engineering (JCEN), 2026.

## Algorithms benchmarked

| Standard | Algorithm | Role |
|---|---|---|
| FIPS 203 | ML-KEM-512 (Kyber-512) | KEM |
| FIPS 204 | ML-DSA-44 (Dilithium-2) | Signature |
| FIPS 205 | SLH-DSA-SHA2-128s (SPHINCS⁺) | Signature |
| FIPS 206 | FN-DSA-512 (Falcon-512) | Signature |
| — | P-256 ECDSA (mbedTLS) | Classical baseline |

## Hardware

- **Board:** STM32 NUCLEO-F446RE
- **MCU:** STM32F446RE — ARM Cortex-M4F @ 84 MHz, 128 KB SRAM, 512 KB Flash
- **Power meter:** Nordic Semiconductor PPK2 (in-series VDD measurement)
- **Implementations:** PQClean reference C (portable, constant-time)
- **Compiler:** `gcc-arm-none-eabi -O2`

## Repository layout

```
.
├── analysis/
│   ├── 01_harmonize_data.py   # truncate & normalise raw CSVs to 370 runs each
│   ├── 02_analyze_all.py      # generate all figures and summary_table.csv
│   └── requirements.txt
├── data/
│   ├── raw/                   # original merged CSVs from two measurement campaigns
│   └── processed/             # harmonised 370-run CSVs (one per algorithm)
├── figures/                   # PNG + EPS figure outputs and summary_table.csv
├── firmware/
│   ├── dataset1/              # STM32CubeIDE project: Dilithium2, Kyber512, P256
│   └── dataset2/              # STM32CubeIDE project: Dilithium2, Falcon512, SPHINCS+
└── paper/
    ├── main.tex               # JCEN LaTeX source (sn-jnl [iicol])
    └── references.bib
```

## Reproducing the analysis

```bash
pip install -r analysis/requirements.txt
python3 analysis/01_harmonize_data.py   # produces data/processed/*.csv
python3 analysis/02_analyze_all.py      # produces figures/*.{png,eps}
```

## Reproducing the firmware

Open `firmware/dataset1/` or `firmware/dataset2/` in **STM32CubeIDE 1.15+**.
Build the `Release` configuration and flash to a NUCLEO-F446RE.
UART output (115200 baud, PA2/PA3) streams one JSON line per operation.
The PPK2 is triggered by GPIO PA5.

## Key results (370 independent trials per operation)

| Algorithm | Keygen (ms) | Sign (ms) | Verify (ms) | Sign CV |
|---|---|---|---|---|
| ML-DSA-44 | 36.8 | 121.3 | 38.1 | 64% |
| FN-DSA-512 | 2923 | 754.9 | 10.5 | 0.2% |
| SLH-DSA-128s | 13851 | 2978 | 103.5 | 0.4% |
| P-256 ECDSA | 394 | 410 | 815 | 0.5% |
| ML-KEM-512 | 10.9 | — | — | — |

## License

Data and analysis scripts: MIT License (see `LICENSE`).  
Firmware third-party components retain their respective licenses:
PQClean (MIT/public domain), FreeRTOS (MIT), mbedTLS (Apache 2.0).
