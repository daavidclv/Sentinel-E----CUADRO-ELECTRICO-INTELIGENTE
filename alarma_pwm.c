/* ============================================================================
 * FASE G — Alarma_pwm.c
 * ========================================================================== */
#include "alarma_pwm.h"
#include "stm32f4xx_hal.h"

/* Reloj efectivo del TIM1 con el SystemClock_Config de Sentinel E:
 *   HCLK = 168 MHz, APB2 = 84 MHz, TIM1 (en APB2) ? x2 ? 168 MHz
 * Estrategia: prescaler fijo para tener un "tick" de 1 MHz, y modificar
 * ARR (Period) para cambiar la frecuencia. Duty siempre 50% (CCR=ARR/2).
 */
#define TIM_TICK_HZ        1000000UL   /* 1 MHz tras prescaler */
#define TIM_PRESCALER      (168U - 1U) /* 168 MHz / 168 = 1 MHz */

static TIM_HandleTypeDef htim_Alarma;
static bool s_initialized = false;

void AlarmaPWM_Init(void)
{
    if (s_initialized) return;

    /* --- Reloj GPIO y TIM1 --- */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    /* --- PE9 como AF1 (TIM1_CH1) --- */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;   /* low: audio, no necesitamos slew */
    gpio.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* --- TIM1 base: prescaler 168 ? 1 MHz; ARR inicial ? 2.5 kHz --- */
    htim_Alarma.Instance               = TIM1;
    htim_Alarma.Init.Prescaler         = TIM_PRESCALER;
    htim_Alarma.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_Alarma.Init.Period            = (TIM_TICK_HZ / 2500U) - 1U;  /* 2.5 kHz */
    htim_Alarma.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_Alarma.Init.RepetitionCounter = 0;
    htim_Alarma.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim_Alarma);

    /* --- CH1: PWM mode 1, polaridad alta, CCR=0 (silencio) --- */
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode      = TIM_OCMODE_PWM1;
    oc.Pulse       = 0;
    oc.OCPolarity  = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode  = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(&htim_Alarma, &oc, TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&htim_Alarma, TIM_CHANNEL_1);

    /* Crítico en TIM1 (timer avanzado): habilitar Main Output Enable.
     * NOTA: a diferencia de tu PWM anterior, NO llamamos a
     * HAL_TIMEx_PWMN_Start() porque usamos el canal normal CH1 (PE9),
     * no el complementario CH1N. */
    __HAL_TIM_MOE_ENABLE(&htim_Alarma);

    s_initialized = true;
}

void AlarmaPWM_SetFrequency(uint32_t freq_hz)
{
    if (!s_initialized) return;
    if (freq_hz < 500U)  freq_hz = 500U;
    if (freq_hz > 8000U) freq_hz = 8000U;

    uint32_t arr = (TIM_TICK_HZ / freq_hz) - 1U;
    __HAL_TIM_SET_AUTORELOAD(&htim_Alarma, arr);

    /* Si está sonando (CCR != 0), recalcular duty al 50% del nuevo ARR */
    if (__HAL_TIM_GET_COMPARE(&htim_Alarma, TIM_CHANNEL_1) != 0U) {
        __HAL_TIM_SET_COMPARE(&htim_Alarma, TIM_CHANNEL_1, (arr + 1U) / 2U);
    }
}

void AlarmaPWM_On(void)
{
    if (!s_initialized) return;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim_Alarma);
    __HAL_TIM_SET_COMPARE(&htim_Alarma, TIM_CHANNEL_1, (arr + 1U) / 2U);
}

void AlarmaPWM_Off(void)
{
    if (!s_initialized) return;
    __HAL_TIM_SET_COMPARE(&htim_Alarma, TIM_CHANNEL_1, 0);
}

void AlarmaPWM_Deinit(void)
{
    if (!s_initialized) return;
    HAL_TIM_PWM_Stop(&htim_Alarma, TIM_CHANNEL_1);
    __HAL_TIM_MOE_DISABLE(&htim_Alarma);
    HAL_TIM_PWM_DeInit(&htim_Alarma);
    __HAL_RCC_TIM1_CLK_DISABLE();
    s_initialized = false;
}