# Paper Improvement Checklist — JCEN Submission

**Paper:** Comprehensive Performance and Variance Benchmarking of NIST PQC Standards on ARM Cortex-M4  
**Target:** Journal of Cryptographic Engineering (JCEN), Springer  
**Current state:** Complete first draft, all TODOs resolved, scaffolding clean.  
**Estimated acceptance probability as-is:** ~40–50% (likely Major Revisions on first submission)

---

## Tier 1 — No new experiments (do first)

### 1. Soften the "first" claims
- [ ] Search `main.tex` for all instances of "first" and qualify each one
- Replace broad "first to present a CV-based variance taxonomy" with scoped version:
  > "the first study on a single commercial MCU to compare CV systematically across all three NIST signature families with n ≥ 100 runs"
- Goal: make every "first" claim falsifiable and defensible under peer review
- **Effort:** 30 min

---

### 2. Add statistical tests to the cross-run section
- [ ] Open `analysis/` and add a script (or extend `02_analyze_all.py`) to run:
  - **Welch's t-test** on sign latency distributions: run 1 vs. run 2
  - Report p-values and Cohen's d (effect size) for keygen, sign, and verify
- [ ] Add one paragraph to Section 6 (Cross-Run Reproducibility) reporting p-values
- Current text says "agreement to three significant figures" — that is engineering language, not statistics
- **Effort:** half day (Python + writing)

---

### 3. Expand Related Work (2022–2024 papers)
- [ ] Search TCHES (tches.iacr.org), ACM TECS (dl.acm.org), and IEEE IoT Journal for:
  - ML-DSA / Dilithium on ARM Cortex-M4 (2022–2024)
  - SLH-DSA / SPHINCS+ on embedded hardware
  - Comparative multi-algorithm PQC benchmarks on MCUs
  - Energy measurement methodology for embedded crypto
- [ ] Aim for 5–8 new citations integrated with differentiation sentences
- Current related work is thin for a journal paper and skews toward 2019–2021
- **Effort:** 1–2 days

---

### 4. Write a dedicated energy methodology subsection
- [ ] Currently the split-window PPK2 algorithm is buried in Section 3.4
- [ ] Expand into its own subsection with pseudocode or a diagram showing:
  - GPIO trigger → PPK2 window → timestamp slice → per-op integral
  - How the `dataset1` vs `dataset2` methods differ and why
- This is a genuine methodological contribution and should be presented as one
- **Effort:** half day

---

### 5. Address the P-256 power anomaly
- [ ] P-256 consistently draws ~109 mW vs. ~91–102 mW for lattice schemes — unexplained
- Likely cause: mbedTLS cache/branch behavior vs. straight-line lattice arithmetic
- [ ] Either investigate in firmware (e.g., check I-cache behavior in STM32 FLASH ACR register) or explicitly flag it in Discussion as a finding worthy of further investigation
- **Effort:** 2–4 hours (investigate) or 30 min (flag it honestly)

---

## Tier 2 — New experiments, same hardware

### 6. Re-run P-256 and Kyber-512 at 1 kSPS with split-window attribution
- [ ] Update Dataset 1 firmware (or create Dataset 3) to:
  - Set PPK2 sample rate to **1 kSPS** (matching Dataset 2)
  - Use per-operation GPIO timestamps and the split-window attribution algorithm
- [ ] Run 370 trials of P-256 (keygen, sign, verify) and Kyber-512 (keygen, encaps, decaps)
- [ ] Re-run `01_harmonize_data.py` and `02_analyze_all.py` with the new data
- **Why this matters:** eliminates the energy methodology inconsistency (the biggest technical weakness). Allows direct cross-algorithm energy comparison without the `I×V×t` caveat.
- **Why P-256 specifically:** resolves whether the ~109 mW anomaly is real or a measurement artifact of the old method
- P-256 runs are fast (~30 min for 370 trials); Kyber is even faster (~10 min)
- **Effort:** 1 day (firmware tweak + run + analysis update)

---

### 7. Run a third ML-DSA-44 and Falcon-512 campaign
- [ ] **Skip SPHINCS+** — CV < 0.4% means a third run adds nothing scientifically
- [ ] Run 370 more trials of ML-DSA-44 (all three ops) using the Dataset 2 firmware
- [ ] Run 370 more trials of Falcon-512 (all three ops)
- [ ] Add Run 3 to the cross-run reproducibility section
- "Three independent experimental campaigns" is a much stronger reproducibility claim than two
- Falcon keygen (44% CV) particularly benefits from an additional run
- **Effort:** 1 day (runs are fast; SPHINCS+ is what takes forever — don't re-run it)

---

## Tier 3 — New platform (bigger lift, highest upside)

### 8. Add a second MCU: Nordic nRF52840
- **Why nRF52840 specifically:**
  - Cortex-M4F @ 64 MHz, ~3.6 V, ~5 mA active — dramatically different energy profile
  - Most widely deployed real-IoT MCU (Bluetooth LE, Thread, Zigbee)
  - Nordic PPK2 was literally designed to profile it — near-zero measurement setup overhead
  - PQClean C code ports with zero changes (same `gcc-arm-none-eabi -O2` toolchain)
  - Reviewers recognize it immediately as a real-world IoT device
- [ ] Port Dataset 2 firmware to nRF52840 (or Nordic nRF5340 DK)
  - Replace STM32 HAL with nRF SDK or Zephyr RTOS equivalents for UART + GPIO
  - DWT cycle counter equivalent: nRF has `DWT_CYCCNT` on Cortex-M4F cores
- [ ] Run 370 trials of all algorithms on nRF52840
- [ ] Add a "Platform Comparison" subsection showing:
  - CV ratios hold across platforms (validates variance taxonomy as general finding)
  - Energy scales with clock speed / supply voltage (expected, but good to show)
  - Which algorithms become impractical at 64 MHz / lower power budget
- **Why this elevates the paper:**
  - "One-board benchmark" → "embedded IoT study with generalizable findings"
  - Empirically validates PQClean's portability claim
  - Opens a new results section that roughly doubles the paper length naturally
- **Effort:** 1–2 weeks (firmware port is the main work; measurements are fast)

---

## Quick reference — priority order

| # | Task | Effort | Impact | Experiments needed? |
|---|---|---|---|---|
| 1 | Soften "first" claims | 30 min | Removes reviewer attack surface | No |
| 2 | Statistical tests (Welch's t-test) | Half day | Upgrades cross-run to journal quality | No |
| 3 | Expand related work 2022–2024 | 1–2 days | Biggest gap reviewers will notice | No |
| 4 | Dedicated energy methodology section | Half day | Showcases split-window as a contribution | No |
| 5 | Address P-256 power anomaly | 2–4 hrs | Removes unexplained finding | No |
| 6 | Re-run P-256 + Kyber at 1 kSPS | 1 day | Fixes core energy inconsistency | **Yes** |
| 7 | Third ML-DSA + Falcon run | 1 day | Strengthens reproducibility | **Yes** |
| 8 | Second platform (nRF52840) | 1–2 weeks | Elevates to generalizable study | **Yes** |

---

## Notes on what NOT to do

- **Do not re-run SPHINCS+** for additional reproducibility runs. CV < 0.4% — a third run is scientifically redundant and takes hours.
- **Do not add a third dataset from scratch** before fixing Dataset 1 energy methodology first (item 6).
- **Do not submit before the related work is expanded** — this is the easiest thing a reviewer can verify and the most common reason for desk rejection at JCEN.

---

## Before submission checklist

- [ ] Switch `\bibliographystyle{unsrt}` → `\bibliographystyle{sn-mathphys}` in Overleaf (requires TeX Live 2023+)
- [ ] Verify FIPS 206 draft status on nist.gov — update bib if finalized
- [ ] Confirm P-256 public key size (64 B in Table 1 — verify this is the raw xy format, not DER-encoded 65 B)
- [ ] Abstract word count: must be 150–250 words (currently ~220 — recheck after edits)
- [ ] Keywords: 6 exactly (currently 6 ✓)
- [ ] Declarations section complete: Funding ✓, Competing interests ✓, Data availability ✓, Author contributions ✓
- [ ] All figures available in EPS format for Springer submission (currently ✓ — both PNG and EPS generated)
