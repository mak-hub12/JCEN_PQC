# STM32 PQC Benchmark - Build Instructions

## Quick Start

### Algorithms Supported
- **P-256** (ECDSA, mbedTLS) - `BENCH_P256`
- **Kyber-512** (ML-KEM, PQClean) - `BENCH_KYBER`
- **Dilithium-2** (ML-DSA-44, PQClean) - `BENCH_DILITHIUM`
- **Falcon-512** (PQClean) - `BENCH_FALCON` ⭐ NEW
- **SPHINCS+-SHA2-128s** (PQClean) - `BENCH_SPHINCS` ⭐ NEW

## Building in STM32CubeIDE

### 1. Open Project
```
File → Import → General → Existing Projects into Workspace
Browse to: /home/mak/Conference_Paper/2026_SATC_PQC_2/stm32_pqc_test_2
Select: ☑ stm32_pqc_test_2
Finish
```

### 2. Select Algorithm to Benchmark
Create or select a build configuration:
```
Project → Build Configurations → Manage → New...
```

### 3. Set Preprocessor Define
```
Right-click project → Properties
→ C/C++ Build → Settings
→ MCU GCC Compiler → Preprocessor → Defined symbols
Add ONE of:
  - BENCH_P256
  - BENCH_KYBER
  - BENCH_DILITHIUM
  - BENCH_FALCON      ⭐ NEW
  - BENCH_SPHINCS     ⭐ NEW
```

### 4. Optional: Configure Test Parameters
```
Add these defines to customize:
  - RUNS=400           (number of iterations)
  - WARMUP=5           (discard first N runs)
  - MEAS_GAP_MS=25     (delay between iterations)
  - CSV_HEADER_ON_BOOT=1
```

### 5. Build
```
Project → Build Configurations → Set Active → [your config]
Project → Build Project
Ctrl+B
```

### 6. Flash to NUCLEO-F446RE
```
Run → Debug As → STM32 Cortex-M C/C++ Application
```

### 7. Monitor Output
- UART2: 115200 baud, 8N1
- CSV format: `alg,op,in_sz,out_sz,cycles,usec,energy_uJ,stack_hwm_words,heap_before,heap_after,...`

## Command-Line Build (Optional)

If using STM32CubeIDE command-line tools:
```bash
# For Falcon
stm32cubeidec --launcher.suppressErrors \
  -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
  -data /path/to/workspace \
  -import /home/mak/Conference_Paper/2026_SATC_PQC_2/stm32_pqc_test \
  -cleanBuild "BENCH_FALCON"

# For SPHINCS+
stm32cubeidec ... -cleanBuild "BENCH_SPHINCS"
```

## Expected Binary Sizes

Approximate flash usage (Release build, -O2):

| Algorithm | Flash (KB) | SRAM (KB) |
|-----------|------------|-----------|
| P-256     | ~80        | ~50       |
| Kyber-512 | ~40        | ~20       |
| Dilithium-2 | ~60      | ~60       |
| Falcon-512 | ~50-70?   | ~45-50?   |
| SPHINCS+  | ~60-80?   | ~55-65?   |

(Final values TBD after compilation)

## Troubleshooting

### Build Error: "api.h: No such file or directory"
**Fix**: Ensure source directories are in include path:
```
Properties → C/C++ General → Paths and Symbols → Includes
Add:
  - ../Middlewares/Third_Party/PQClean/FALCON512_clean
  - ../Middlewares/Third_Party/PQClean/SPHINCSSHA2128SSIMPLE_clean
  - ../Middlewares/Third_Party/PQClean/common
```

### Linker Error: "undefined reference to PQCLEAN_..."
**Fix**: Ensure .c files are included in build:
```
Check that folders are NOT excluded:
Right-click folder → Properties → C/C++ Build → Exclude from Build (uncheck)
```

### Runtime: Hard Fault / Stack Overflow
**Fix**: Increase stack size in Core/Src/main.c:
```c
#define STACK_FALCON    10240   // Increase if needed
#define STACK_SPHINCS   12288   // Increase if needed
```

### Runtime: SPHINCS+ Signature Array Too Large
**Fix**: Signature is 7,856 bytes. If stack is constrained, consider static allocation:
```c
static uint8_t sig[PQCLEAN_SPHINCSSHA2128SSIMPLE_CLEAN_CRYPTO_BYTES];
```

## Notes

- **RNG**: Uses deterministic PRNG (port/randombytes.c) for reproducible benchmarks
  - ⚠️ **NOT CRYPTOGRAPHICALLY SECURE** - for benchmarking only!
  - Replace with TRNG for production use

- **Optimization**: Use `-O2` or `-Os` for realistic performance
  - Debug builds (`-O0`) will be significantly slower

- **Stack monitoring**: Check `stack_hwm_words` in CSV output
  - Should be less than allocated stack size
  - If close to limit, increase STACK_* define

## Hardware Setup

### Minimal (firmware only)
- NUCLEO-F446RE board
- USB cable (ST-LINK + UART)
- Serial terminal (115200 baud)

### Full measurement setup (Paper #2)
- NUCLEO-F446RE board
- Nordic PPK2 (Power Profiler Kit 2)
- Jumper wires:
  - PPK2 VOUT → NUCLEO JP6 IDD left pin
  - PPK2 VIN → NUCLEO JP6 IDD right pin
  - PPK2 GND → NUCLEO GND
  - PPK2 D0 (trigger) → NUCLEO PA5 (optional, for sync)
- I2C sensors (optional):
  - TMP117 @ 0x48 (temperature)
  - SHT31-D @ 0x44 (temp + humidity)

## Paper Reference

This code implements the methodology from:
> Austin McGuire, "Practical Post-Quantum Cryptography on an MCU-Class IoT Platform:
> Latency, Energy, and Stack Measurements for Kyber-512, Dilithium-2, and P-256",
> SATC 2026 (to appear)

Extended for Falcon-512 and SPHINCS+-SHA2-128s for second thesis paper.
