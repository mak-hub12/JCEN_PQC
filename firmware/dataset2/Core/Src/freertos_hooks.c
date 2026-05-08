#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;

static void hook_print(const char *s)
{
    extern UART_HandleTypeDef huart2;
    if (!s) return;
    HAL_UART_Transmit(&huart2, (uint8_t*)s, (uint16_t)strlen(s), 100);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    char buf[96];
    int n = snprintf(buf, sizeof buf,
                     "[rtos][FATAL] stack overflow in %s\r\n",
                     pcTaskName ? pcTaskName : "(unknown)");
    if (n > 0) hook_print(buf);
    taskDISABLE_INTERRUPTS();
    for(;;);
}


void vApplicationMallocFailedHook( void )
{
    const char *s = "[rtos][FATAL] malloc failed\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)s, strlen(s), 200);
    while (1) { HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3); HAL_Delay(150); }
}
