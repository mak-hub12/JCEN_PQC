#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stddef.h>

/* Print the CSV header line */
void telemetry_print_header(void);

/* Emit one CSV data line (sensors are appended from internal cache) */
void csv_emit(const char *alg, const char *op,
              size_t bytes_in, size_t bytes_out,
              uint32_t cycles, uint32_t usec, uint32_t energy_uJ,
              uint32_t stack_hwm_words,
              uint32_t heap_before, uint32_t heap_after);

/* ---- Sensor cache setters (called by SensorTask) ----
   Units:
     - tmp117_cC : centi-degC (25.18°C -> 2518)
     - sht31_cC  : centi-degC
     - rh_dpermil: deci-percent RH (63.4% -> 634)  */
void telemetry_env_set_tmp117(int32_t centiC);
void telemetry_env_set_sht31(int32_t centiC, int32_t rh_dpermil);

#endif /* TELEMETRY_H */
