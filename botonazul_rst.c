/*------------------------------------------------------------------------------
 *  Boton azul (PC13) - Wake-up del LPWR_Mode
 *
 *  ESTADO ACTUAL: FASE C - el callback envia flag 0x20 al hilo LPWR_Mode
 *                          con debounce software de 200ms para evitar rebotes
 *----------------------------------------------------------------------------*/

#include "stm32f4xx_hal.h"
#include "rtc.h"
#include "cmsis_os2.h"

GPIO_InitTypeDef GPIO_InitStruct;

extern osThreadId_t TID_LPWR;

/* Debounce: tiempo minimo entre pulsaciones validas (ms) */
#define BOTON_DEBOUNCE_MS    500U


void BotonAzul_Init(void) { //PC13
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    /* Debounce software: ignorar pulsaciones demasiado seguidas.
       PC13 no tiene debounce por hardware y un pulsador mecanico genera
       multiples flancos en pocos ms. Sin esto, una pulsacion provoca
       N interrupciones y N flags acumuladas en LPWR. */
    static uint32_t t_ultima_pulsacion = 0U;
    uint32_t ahora;

    if (GPIO_Pin == GPIO_PIN_13) {
        ahora = HAL_GetTick();

        if ((ahora - t_ultima_pulsacion) < BOTON_DEBOUNCE_MS) {
            return;   /* rebote, ignorar */
        }
        t_ultima_pulsacion = ahora;

        /* === FASE C === Avisar a LPWR_Mode (flag 0x20 = ciclo completo) */
        osThreadFlagsSet(TID_LPWR, 0x20);

        /* === FASE OPCIONAL === (codigo original conservado)
         * RTC_ConfTime(0, 0, 0);
         * RTC_ConfDate(RTC_WEEKDAY_SATURDAY, 0, 1, 100);
         */
    }
}