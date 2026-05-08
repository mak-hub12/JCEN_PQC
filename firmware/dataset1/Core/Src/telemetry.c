#include "telemetry.h"

#include "main.h"
#include "usart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

/* Externals from CubeMX */
extern UART_HandleTypeDef huart2;

/* Compile-time strings (safe defaults) */
#ifndef BOARD_NAME
#define BOARD_NAME "UNKNOWN_BOARD"
#endif
#ifndef MCU_NAME
#define MCU_NAME "UNKNOWN_MCU"
#endif
#ifndef TOOLCHAIN_STR
#define TOOLCHAIN_STR "gcc-arm-none-eabi"
#endif
#ifndef OFLAG_STR
#define OFLAG_STR "O2"
#endif
#ifndef LTO_STR
#define LTO_STR "off"
#endif
#ifndef LIB_COMMIT_STR
#define LIB_COMMIT_STR "unknown"
#endif
#ifndef FW_GIT_STR
#define FW_GIT_STR "nogit"
#endif

/* --- Tiny helpers --- */
static inline uint32_t mhz(void) {
    return (uint32_t)(SystemCoreClock / 1000000UL);
}
static inline uint32_t ms_since_boot(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
static void uart_write(const char *s) {
    if (!s) return;
    HAL_UART_Transmit(&huart2, (uint8_t*)s, (uint16_t)strlen(s), 200);
}

/* =========================================================
 *     SENSOR CACHE (set by SensorTask, read by csv_emit)
 * ========================================================= */
typedef struct {
    /* centi-degC and 0.1%RH; INT32_MIN means “no data yet” */
    volatile int32_t tmp117_cC;
    volatile int32_t sht31_cC;
    volatile int32_t sht31_rh_dp;
} tely_env_t;

static tely_env_t g_env = { INT32_MIN, INT32_MIN, INT32_MIN };

void telemetry_env_set_tmp117(int32_t centiC) { g_env.tmp117_cC = centiC; }
void telemetry_env_set_sht31 (int32_t centiC, int32_t rh_dp) {
    g_env.sht31_cC  = centiC;
    g_env.sht31_rh_dp = rh_dp;
}

/* =========================================================
 *                      CSV functions
 * ========================================================= */
void telemetry_print_header(void)
{
    const char *hdr =
        "board,mcu,freq_mhz,timestamp_ms,"
        "alg,op,bytes_in,bytes_out,cycles,usec,energy_uJ,"
        "stack_hwm_words,heap_before,heap_after,"
        "toolchain,Oflag,lto,lib_commit,fw_git,"
        "tmp117_cC,sht31_cC,sht31_rh_dpermil\r\n";
    uart_write(hdr);
}

void csv_emit(const char *alg, const char *op,
              size_t bytes_in, size_t bytes_out,
              uint32_t cycles, uint32_t usec, uint32_t energy_uJ,
              uint32_t stack_hwm_words,
              uint32_t heap_before, uint32_t heap_after)
{
    /* Snapshot the cache ONCE (no I2C here). */
    int32_t t117 = g_env.tmp117_cC;
    int32_t s31T = g_env.sht31_cC;
    int32_t s31H = g_env.sht31_rh_dp;

    char line[320];
    int n = snprintf(line, sizeof line,
        "%s,%s,%lu,%lu,"
        "%s,%s,%u,%u,%lu,%lu,%lu,"
        "%lu,%lu,%lu,"
        "%s,%s,%s,%s,%s,",
        BOARD_NAME, MCU_NAME,
        (unsigned long)mhz(), (unsigned long)ms_since_boot(),
        alg ? alg : "", op ? op : "",
        (unsigned)bytes_in, (unsigned)bytes_out,
        (unsigned long)cycles, (unsigned long)usec, (unsigned long)energy_uJ,
        (unsigned long)stack_hwm_words,
        (unsigned long)heap_before, (unsigned long)heap_after,
        TOOLCHAIN_STR, OFLAG_STR, LTO_STR, LIB_COMMIT_STR, FW_GIT_STR
    );
    if (n < 0) return;

    /* Append sensor columns (as integers). If missing, print empty fields. */
    if (t117 != INT32_MIN) {
        n += snprintf(line + n, sizeof(line) - n, "%ld,", (long)t117);
    } else {
        n += snprintf(line + n, sizeof(line) - n, ",");
    }

    if (s31T != INT32_MIN && s31H != INT32_MIN) {
        n += snprintf(line + n, sizeof(line) - n, "%ld,%ld\r\n", (long)s31T, (long)s31H);
    } else {
        n += snprintf(line + n, sizeof(line) - n, ",\r\n");
    }

    if (n > 0) uart_write(line);
}
