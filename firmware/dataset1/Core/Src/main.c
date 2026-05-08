/* Crypto bench main: P-256 (mbedTLS), MLKEM-512 (Kyber), MLDSA-44 (Dilithium)
 *
 * Define exactly one in your build:
 *   BENCH_P256  or  BENCH_KYBER  or  BENCH_DILITHIUM
 *
 * Optional flags:
 *   NO_TMP117, NO_SHT31        : sensors are ignored by SensorTask
 *   RUNS=1000                  : total iterations
 *   WARMUP=5                   : iterations to discard before csv emit
 *   MEAS_GAP_MS=0              : delay between iterations
 *   CSV_HEADER_ON_BOOT=1       : print CSV header once at boot
 */

#include "main.h"
#include "cmsis_os.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* CSV helper (adds timestamp_ms internally, holds sensor cache setters) */
#include "telemetry.h"

/* ---------- Config (override with -D) ---------- */
#ifndef RUNS
#define RUNS            100
#endif
#ifndef WARMUP
#define WARMUP          5
#endif
#ifndef MEAS_GAP_MS
#define MEAS_GAP_MS     25
#endif
#ifndef CSV_HEADER_ON_BOOT
#define CSV_HEADER_ON_BOOT 1
#endif

/* ---------- Stacks ---------- */
#ifndef STACK_P256
#define STACK_P256      4096
#endif
#ifndef STACK_KYBER
#define STACK_KYBER     4096
#endif
#ifndef STACK_DILITHIUM
#define STACK_DILITHIUM 15360
#endif
#ifndef STACK_SENSOR
#define STACK_SENSOR    1024
#endif

/* ---------- Sensor/Crypto Task Priorities ---------- */
#define PRIO_SENSOR     (tskIDLE_PRIORITY + 3)
#define PRIO_CRYPTO     (tskIDLE_PRIORITY + 2)

/* forward decl */
static void SensorTask(void *arg);

/* ---------- mbedTLS (P-256) ---------- */
#if defined(BENCH_P256)
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ctr_drbg.h"
#endif

/* ---------- PQClean headers (only when needed) ---------- */
#if defined(BENCH_KYBER)
  #include "MLKEM512_clean/api.h"
#endif
#if defined(BENCH_DILITHIUM)
  #include "MLDSA44_clean/api.h"
#endif

/* ---------- FreeRTOS HWM fallback ---------- */
#ifndef INCLUDE_uxTaskGetStackHighWaterMark
#define INCLUDE_uxTaskGetStackHighWaterMark 0
#endif
#if (INCLUDE_uxTaskGetStackHighWaterMark == 0)
static inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask) { (void)xTask; return 0; }
#endif

/* ---------- UART helpers ---------- */
static void uart_puts(const char *s) {
    HAL_UART_Transmit(&huart2, (uint8_t*)s, (uint16_t)strlen(s), 200);
}
static void uart_u32(const char *tag, uint32_t v) {
    char b[64];
    int n = snprintf(b, sizeof b, "%s%lu\r\n", tag, (unsigned long)v);
    HAL_UART_Transmit(&huart2, (uint8_t*)b, n, 200);
}

/* ---------- DWT timing ---------- */
static inline void dwt_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline uint32_t dwt_cycles(void) { return DWT->CYCCNT; }
static inline uint32_t us_from_cycles(uint32_t cyc) {
    return (uint32_t)((uint64_t)cyc * 1000000ULL / (uint64_t)SystemCoreClock);
}

/* ---------- PA5 scope/PPK2 marker ---------- */
static inline void mark_init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_5;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}
static inline void mark_on(void)  { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); }
static inline void mark_off(void) { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); }

/* ---------- Forward decls ---------- */
static void SensorTask(void *arg);

#if defined(BENCH_P256)
static void P256Task(void *arg);
#endif
#if defined(BENCH_KYBER)
static void KyberTask(void *arg);
#endif
#if defined(BENCH_DILITHIUM)
static void DilithiumTask(void *arg);
#endif

void SystemClock_Config(void);
void Error_Handler(void);

/* ---------- I2C scan (debug) ---------- */
static void i2c1_scan_quick(void) {
    extern I2C_HandleTypeDef hi2c1;
    uint8_t have44=0, have48=0;
    uart_puts("[i2c] quick scan...\r\n");
    for (uint8_t a=0x08; a<=0x77; a++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, a<<1, 1, 5) == HAL_OK) {
            char line[40];
            int n = snprintf(line, sizeof line, "[i2c] found 0x%02X\r\n", a);
            HAL_UART_Transmit(&huart2, (uint8_t*)line, n, 100);
            if (a==0x44) have44=1;
            if (a==0x48) have48=1;
        }
    }
    char line[64];
    int n = snprintf(line, sizeof line, "[i2c] scan done (SHT31:%s TMP117:%s)\r\n",
                     have44?"yes":"no", have48?"yes":"no");
    HAL_UART_Transmit(&huart2, (uint8_t*)line, n, 100);
}

/* =========================================================
 *                        main()
 * ========================================================= */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_I2C1_Init();          /* init I2C before tasks */
    dwt_init();
    mark_init();

    uart_puts("\r\n[BOOT] NUCLEO-F446RE, UART2 115200\r\n");
    i2c1_scan_quick();

#if CSV_HEADER_ON_BOOT
    telemetry_print_header();
#endif

    /* Self-doc config line */
    {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "[cfg] RUNS=%d WARMUP=%d GAP=%dms TMP117=%s SHT31=%s\r\n",
                 RUNS, WARMUP, MEAS_GAP_MS,
    #ifdef NO_TMP117
                 "off",
    #else
                 "on",
    #endif
    #ifdef NO_SHT31
                 "off");
    #else
                 "on");
    #endif
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)strlen(msg), 100);
    }

    extern size_t xPortGetFreeHeapSize(void);
    uart_u32("[rtos] free heap pre-create: ", (uint32_t)xPortGetFreeHeapSize());

    /* Start sensor task first (higher priority this time) — safe even if NO_* are defined */
    BaseType_t ok;
    ok = xTaskCreate(SensorTask, "sensor", 512 /*or 768*/, NULL, PRIO_SENSOR, NULL);
    uart_puts(ok==pdPASS ? "[rtos] xTaskCreate Sensor -> PASS\r\n"
                         : "[rtos] xTaskCreate Sensor -> FAIL\r\n");

    /* task delay while sensors get a read */
    vTaskDelay(pdMS_TO_TICKS(40));


    /* Create exactly one crypto task */
#if defined(BENCH_P256)
    ok = xTaskCreate(P256Task, "p256", STACK_P256, NULL, PRIO_CRYPTO, NULL);
    uart_puts("[rtos] xTaskCreate P256 -> "); uart_puts(ok==pdPASS ? "PASS\r\n" : "FAIL\r\n");
#elif defined(BENCH_KYBER)
    ok = xTaskCreate(KyberTask, "kyber", STACK_KYBER, NULL, PRIO_CRYPTO, NULL);
    uart_puts("[rtos] xTaskCreate Kyber -> "); uart_puts(ok==pdPASS ? "PASS\r\n" : "FAIL\r\n");
#elif defined(BENCH_DILITHIUM)
    ok = xTaskCreate(DilithiumTask, "dilithium", STACK_DILITHIUM, NULL, PRIO_CRYPTO, NULL);
    uart_puts("[rtos] xTaskCreate Dilithium -> "); uart_puts(ok==pdPASS ? "PASS\r\n" : "FAIL\r\n");
#else
    ok = xTaskCreate(P256Task, "p256", STACK_P256, NULL, PRIO_CRYPTO, NULL);
    uart_puts("[rtos] xTaskCreate P256(default) -> "); uart_puts(ok==pdPASS ? "PASS\r\n" : "FAIL\r\n");
#endif

uart_puts("[rtos] starting scheduler...\r\n");
vTaskStartScheduler();

    /* If we get here, Idle/Timer tasks couldn’t start */
    uart_puts("[rtos][FATAL] vTaskStartScheduler returned. Heap too small?\r\n");
    while (1) {
        HAL_Delay(250);
    }
}

/* =========================================================
 *                      SensorTask
 *  - Owns I2C. Samples sensors every 250 ms.
 *  - Stores results in telemetry cache (no I2C in csv_emit).
 * ========================================================= */
static bool dbg_tmp_printed = false;
static bool dbg_sht_printed = false;
static void i2c_bus_recover(I2C_HandleTypeDef *hi2c)
{
    /* Best-effort software reset on STM32F4 I2C */
    __HAL_I2C_DISABLE(hi2c);
    HAL_Delay(1);
    __HAL_I2C_ENABLE(hi2c);
    /* Re-init peripheral if needed */
    if (hi2c->State != HAL_I2C_STATE_READY) {
        HAL_I2C_DeInit(hi2c);
        HAL_I2C_Init(hi2c);
    }
}

static void SensorTask(void *arg)
{
    (void)arg;
    extern I2C_HandleTypeDef hi2c1;

    const TickType_t period = pdMS_TO_TICKS(250);

    /* Device presence flags (lazy-checked) */
    bool have_tmp = false, have_sht = false;

    for (;;) {
#ifndef NO_TMP117
        if (!have_tmp) {
            have_tmp = (HAL_I2C_IsDeviceReady(&hi2c1, (0x48u<<1), 1, 5) == HAL_OK);
        }
        if (have_tmp) {
            /* TMP117: read temperature register 0x00, big endian; 7.8125 mC/LSB */
            uint8_t buf[2];
            if (HAL_I2C_Mem_Read(&hi2c1, (0x48u<<1), 0x00, I2C_MEMADD_SIZE_8BIT, buf, 2, 20) == HAL_OK) {
                int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
                int32_t centiC = (int32_t)((int64_t)raw * 78125LL / 100000LL); /* 0.78125°C/LSB -> cC */
                telemetry_env_set_tmp117(centiC);
                if (!dbg_tmp_printed) {
                    char b[64];
                    int n = snprintf(b, sizeof b, "[sensor] TMP117 first: %ld cC\r\n", (long)centiC);
                    HAL_UART_Transmit(&huart2, (uint8_t*)b, n, 100);
                    dbg_tmp_printed = true;
                }
            } else {
                i2c_bus_recover(&hi2c1);
            }
        }
#endif

#ifndef NO_SHT31
        if (!have_sht) {
            have_sht = (HAL_I2C_IsDeviceReady(&hi2c1, (0x44u<<1), 1, 5) == HAL_OK);
            if (have_sht) {
                uint8_t rst[2] = {0x30, 0xA2}; (void)HAL_I2C_Master_Transmit(&hi2c1, (0x44u<<1), rst, 2, 5);
                uint8_t off[2] = {0x30, 0x41}; (void)HAL_I2C_Master_Transmit(&hi2c1, (0x44u<<1), off, 2, 5);
            }
        }
        if (have_sht) {
            /* One single-shot, high repeatability, no clock-stretch */
            uint8_t cmd[2] = {0x24, 0x00};
            if (HAL_I2C_Master_Transmit(&hi2c1, (0x44u<<1), cmd, 2, 10) == HAL_OK) {
                vTaskDelay(pdMS_TO_TICKS(20)); /* wait measurement */
                uint8_t buf[6] = {0};
                if (HAL_I2C_Master_Receive(&hi2c1, (0x44u<<1), buf, 6, 20) == HAL_OK) {
                    uint16_t rt = (uint16_t)((buf[0] << 8) | buf[1]);
                    uint16_t rh = (uint16_t)((buf[3] << 8) | buf[4]);
                    if (rt && rh && rt != 0xFFFF && rh != 0xFFFF) {
                        int32_t t_cC  = -4500 + (int32_t)((17500LL * rt) / 65535LL);  /* °C*100 */
                        int32_t rh_dp = (int32_t)((1000LL  * rh) / 65535LL);          /* 0.1% */
                        if (rh_dp < 0)    rh_dp = 0;
                        if (rh_dp > 1000) rh_dp = 1000;
                        telemetry_env_set_sht31(t_cC, rh_dp);
                        if (!dbg_sht_printed) {
                            char b[80];
                            int n = snprintf(b, sizeof b, "[sensor] SHT31 first: %ld cC, %ld dpermil\r\n",
                                             (long)t_cC, (long)rh_dp);
                            HAL_UART_Transmit(&huart2, (uint8_t*)b, n, 100);
                            dbg_sht_printed = true;
                        }
                    }
                } else {
                    i2c_bus_recover(&hi2c1);
                }
            } else {
                i2c_bus_recover(&hi2c1);
            }
        }
#endif

        vTaskDelay(period);
    }
}

/* =========================================================
 *                   P-256 (ECDSA) Task
 * ========================================================= */
#if defined(BENCH_P256)
/* Minimal deterministic “entropy” for ctr_drbg_seed() to make runs reproducible. */
static int fixed_entropy(void *p, unsigned char *buf, size_t len) {
    (void)p;
    static uint32_t s = 0x1A2B3C4D;
    for (size_t i = 0; i < len; i++) {
        s = s * 1664525u + 1013904223u;     /* LCG */
        buf[i] = (unsigned char)(s >> 24);
    }
    return 0;
}

static void P256Task(void *arg)
{
    (void)arg;
    const char *ALG = "p256";
    uart_puts("[task] P256Task start\r\n");

    static const uint8_t d_bin[32] = {
        0x52,0x6C,0x80,0x36,0x7E,0x11,0x4D,0xB5,0x9C,0xD7,0xA1,0xF3,0x09,0x12,0x34,0x56,
        0x9A,0xBC,0xDE,0xF0,0x10,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC
    };
    static const unsigned char msg[] = "hello from p256";

    mbedtls_ecp_group grp;       mbedtls_ecp_group_init(&grp);
    mbedtls_mpi d;               mbedtls_mpi_init(&d);
    mbedtls_ecp_point Q;         mbedtls_ecp_point_init(&Q);
    mbedtls_ecdsa_context ecdsa; mbedtls_ecdsa_init(&ecdsa);
    mbedtls_ctr_drbg_context ctr; mbedtls_ctr_drbg_init(&ctr);

    int ret = 0;

    if ((ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1)) != 0) goto out;
    if ((ret = mbedtls_mpi_read_binary(&d, d_bin, sizeof d_bin)) != 0)       goto out;
    if ((ret = mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, NULL, NULL)) != 0)      goto out;

    if ((ret = mbedtls_ecp_group_copy(&ecdsa.grp, &grp)) != 0)               goto out;
    if ((ret = mbedtls_mpi_copy(&ecdsa.d, &d)) != 0)                         goto out;
    if ((ret = mbedtls_ecp_copy(&ecdsa.Q, &Q)) != 0)                         goto out;

    static const unsigned char seed[] = "p256-fixed-seed-for-dissertation";
    if ((ret = mbedtls_ctr_drbg_seed(&ctr, fixed_entropy, NULL, seed, sizeof(seed))) != 0) goto out;

    for (uint32_t i = 0; i < (uint32_t)RUNS; i++) {
        uint32_t hb, c0, c1;

        /* keygen (Q=dG) cost */
        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, NULL, NULL);
        c1 = dwt_cycles(); mark_off();
        if (ret) break;
        if (i >= (uint32_t)WARMUP)
            csv_emit(ALG, "keygen", 0, 64, c1-c0, us_from_cycles(c1-c0), 0,
                     uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());

        /* hash */
        unsigned char hash[32];
        mbedtls_sha256(msg, sizeof(msg)-1, hash, 0);

        /* sign */
        unsigned char sig[80]; size_t sig_len = 0;
        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = mbedtls_ecdsa_write_signature(&ecdsa,
                MBEDTLS_MD_SHA256, hash, sizeof hash,
                sig, &sig_len, mbedtls_ctr_drbg_random, &ctr);
        c1 = dwt_cycles(); mark_off();
        if (ret) break;
        if (i >= (uint32_t)WARMUP)
            csv_emit(ALG, "sign", sizeof(hash), sig_len, c1-c0, us_from_cycles(c1-c0), 0,
                     uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());

        /* verify */
        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = mbedtls_ecdsa_read_signature(&ecdsa, hash, sizeof hash, sig, sig_len);
        c1 = dwt_cycles(); mark_off();
        if (i >= (uint32_t)WARMUP)
            csv_emit(ALG, (ret==0) ? "verify_ok" : "verify_fail",
                     sizeof(hash), 0, c1-c0, us_from_cycles(c1-c0), 0,
                     uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());

        if (MEAS_GAP_MS) vTaskDelay(pdMS_TO_TICKS(MEAS_GAP_MS));
        taskYIELD();
    }

out:
    uart_puts("[p256] done\r\n");
    mbedtls_ctr_drbg_free(&ctr);
    mbedtls_ecdsa_free(&ecdsa);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    vTaskDelete(NULL);
}
#endif /* BENCH_P256 */

/* =========================================================
 *                   MLKEM-512 (Kyber) Task
 * ========================================================= */
#if defined(BENCH_KYBER)
static void KyberTask(void *arg)
{
    (void)arg;
    const char *ALG = "kyber512";
    uart_puts("[task] KyberTask start\r\n");

    const size_t PKB = PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES;
    const size_t SKB = PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES;
    const size_t CTB = PQCLEAN_MLKEM512_CLEAN_CRYPTO_CIPHERTEXTBYTES;
    const size_t SSB = PQCLEAN_MLKEM512_CLEAN_CRYPTO_BYTES;

    uint8_t pk[PKB], sk[SKB], ct[CTB], ss1[SSB], ss2[SSB];
    int ret;

    for (int i = 0; i < RUNS; i++) {
        uint32_t hb, c0, c1;

        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);
        c1 = dwt_cycles(); mark_off();
        if (i >= WARMUP) csv_emit(ALG, "keygen", 0, PKB, c1-c0, us_from_cycles(c1-c0), 0,
                                  uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());
        if (ret) break;

        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_enc(ct, ss1, pk);
        c1 = dwt_cycles(); mark_off();
        if (i >= WARMUP) csv_emit(ALG, "encaps", PKB, CTB, c1-c0, us_from_cycles(c1-c0), 0,
                                  uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());
        if (ret) break;

        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = PQCLEAN_MLKEM512_CLEAN_crypto_kem_dec(ss2, ct, sk);
        c1 = dwt_cycles(); mark_off();
        if (i >= WARMUP) csv_emit(ALG, "decaps", CTB, SSB, c1-c0, us_from_cycles(c1-c0), 0,
                                  uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());
        if (ret) break;

        if (MEAS_GAP_MS) vTaskDelay(pdMS_TO_TICKS(MEAS_GAP_MS));
    }

    uart_puts("[kyber512] done\r\n");
    vTaskDelete(NULL);
}
#endif /* BENCH_KYBER */

/* =========================================================
 *                 MLDSA-44 (Dilithium2) Task
 * ========================================================= */
#if defined(BENCH_DILITHIUM)
static void DilithiumTask(void *arg)
{
    (void)arg;
    const char *ALG = "dilithium2";
    uart_puts("[task] DilithiumTask start\r\n");

    const size_t PKB = PQCLEAN_MLDSA44_CLEAN_CRYPTO_PUBLICKEYBYTES;
    const size_t SKB = PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES;
    const size_t SSB = PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES;

    uint8_t pk[PKB], sk[SKB];
    uint8_t sig[SSB]; size_t siglen;
    int ret;

    for (int i = 0; i < RUNS; i++) {
        uint32_t hb, c0, c1;

        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = PQCLEAN_MLDSA44_CLEAN_crypto_sign_keypair(pk, sk);
        c1 = dwt_cycles(); mark_off();
        if (i >= WARMUP) csv_emit(ALG, "keygen", 0, PKB, c1-c0, us_from_cycles(c1-c0), 0,
                                  uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());
        if (ret) break;

        siglen = 0;
        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = PQCLEAN_MLDSA44_CLEAN_crypto_sign_signature(
                  sig, &siglen, (const uint8_t*)"dilithium test", 14, sk);
        c1 = dwt_cycles(); mark_off();
        if (i >= WARMUP) csv_emit(ALG, "sign", 14, siglen, c1-c0, us_from_cycles(c1-c0), 0,
                                  uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());
        if (ret) break;

        hb = xPortGetFreeHeapSize();
        mark_on(); c0 = dwt_cycles();
        ret = PQCLEAN_MLDSA44_CLEAN_crypto_sign_verify(
                  sig, siglen, (const uint8_t*)"dilithium test", 14, pk);
        c1 = dwt_cycles(); mark_off();
        if (i >= WARMUP) csv_emit(ALG, (ret==0) ? "verify_ok" : "verify_fail",
                                  siglen, 0, c1-c0, us_from_cycles(c1-c0), 0,
                                  uxTaskGetStackHighWaterMark(NULL), hb, xPortGetFreeHeapSize());
        if (ret) break;

        if (MEAS_GAP_MS) vTaskDelay(pdMS_TO_TICKS(MEAS_GAP_MS));
    }

    uart_puts("[dilithium2] done\r\n");
    vTaskDelete(NULL);
}
#endif /* BENCH_DILITHIUM */

/* ---------- Clock config (known-good) ---------- */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 16;
    RCC_OscInitStruct.PLL.PLLN            = 336;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ            = 2;
    RCC_OscInitStruct.PLL.PLLR            = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
