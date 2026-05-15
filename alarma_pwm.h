/* ============================================================================
 * FASE G — AlarmaA INTELIGENTE
 * Alarma_pwm.h — Driver PWM bare-metal para buzzer pasivo
 *
 * HW: Buzzer pasivo en PE9 (TIM1_CH1, AF1)
 * Reloj: TIM1 en APB2 ? 168 MHz efectivos (APB2 timers x2)
 * ========================================================================== */
#ifndef Alarma_PWM_H
#define Alarma_PWM_H

#include <stdint.h>
#include <stdbool.h>

/* Inicializa TIM1_CH1 sobre PE9. Idempotente. */
void AlarmaPWM_Init(void);

/* Fija frecuencia del tono [Hz]. Rango útil: 500..8000 Hz. */
void AlarmaPWM_SetFrequency(uint32_t freq_hz);

/* Arranca PWM con duty 50% (máxima energía acústica en piezo). */
void AlarmaPWM_On(void);

/* Para PWM (CCR=0, salida a 0). */
void AlarmaPWM_Off(void);

/* Para por completo el timer (uso opcional al entrar Stop, aunque
   con nuestra política de inhibir Stop con Alarmaa activa no es estrictamente
   necesario). */
void AlarmaPWM_Deinit(void);

#endif /* Alarma_PWM_H */