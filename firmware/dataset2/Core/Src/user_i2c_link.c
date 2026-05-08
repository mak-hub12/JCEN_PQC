#include "stm32f4xx_hal.h"
#include "i2c.h"              // brings in hi2c1
#include "telemetry.h"

I2C_HandleTypeDef* telemetry_get_i2c(void) {
    return &hi2c1;            // use I2C1 for TMP117 + SHT31-D
}
