/* ============================================================================
 * FASE G — alarma_motor.h
 * Motor de alarma de temperatura con histéresis y patrón sirena.
 * ========================================================================== */
#ifndef ALARMA_MOTOR_H
#define ALARMA_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

/* Umbral por defecto (sobrescribible por A vía SET_UMBRAL_TEMP). */
#define ALARMA_TEMP_THRESHOLD_DEFAULT_C   28.0f

/* Histéresis: la alarma se desactiva por debajo de (umbral - HYSTERESIS). */
#define ALARMA_TEMP_HYSTERESIS_C          1.0f

/* Inicializa el motor: TIM1, thread, estado. Llamar una vez al arranque. */
int Motor_Alarma_Init(void);

/* Llamado por TID_TEMP cada vez que hay nueva lectura. */
void Motor_Alarma_OnNewTemperature(float t_celsius);

/* Llamado por el parser UART cuando llega SET_UMBRAL_TEMP desde A. */
void Motor_Alarma_SetThreshold(float t_celsius);

/* Consulta de estado (la usa el orquestador para inhibir Stop Mode). */
bool Motor_Alarma_IsActive(void);

/* Para diagnóstico/web: devuelve umbral actual. */
float Motor_Alarma_GetThreshold(void);

#endif /* ALARMA_MOTOR_H */