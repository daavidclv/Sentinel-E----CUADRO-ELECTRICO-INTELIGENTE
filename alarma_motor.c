/* ============================================================================
 * FASE G — alarma_motor.c
 * Motor de alarma con histéresis y patrón sirena.
 * ========================================================================== */
#include "alarma_motor.h"
#include "alarma_pwm.h"
#include "cmsis_os2.h"
#include <stdio.h>   /* ITM printf */
#include "LEDs.h"     

/* Cuando integremos UART/fibra: */
/* #include "fibra.h"   // o el módulo equivalente para enviar EVT_ALARMA_TEMP */

/* ---- Flags del thread de alarma ---- */
#define FLAG_ALARMA_TICK        (1U << 0)   /* tick del patrón sirena */
#define FLAG_ALARMA_ACTIVATE    (1U << 1)   /* activar alarma         */
#define FLAG_ALARMA_DEACTIVATE  (1U << 2)   /* desactivar alarma      */

#define ALARMA_PATTERN_MS       80U
#define ALARMA_FREQ_LOW         2000U
#define ALARMA_FREQ_HIGH        3500U
#define ALARMA_FREQ_STEP        100U


static osThreadId_t   s_tid_alarma   = NULL;
static osTimerId_t    s_tim_pattern  = NULL;
static osMutexId_t    s_mtx_state    = NULL;

static volatile bool  s_active       = false;
static float          s_threshold_c  = ALARMA_TEMP_THRESHOLD_DEFAULT_C;  /* 28.0 °C */

static void AlarmaThread(void *arg);
static void AlarmaPatternTimerCb(void *arg);

/* -------------------------------------------------------------------------- */
int Motor_Alarma_Init(void)
{
    AlarmaPWM_Init();
    AlarmaPWM_Off();

    s_mtx_state = osMutexNew(NULL);
    if (s_mtx_state == NULL) return -1;

    /* Timer periódico de patrón. Solo se arranca cuando la alarma está activa. */
    s_tim_pattern = osTimerNew(AlarmaPatternTimerCb, osTimerPeriodic, NULL, NULL);
    if (s_tim_pattern == NULL) return -2;

    const osThreadAttr_t attr = {
        .name       = "AlarmaThread",
        .stack_size = 512U,
        .priority   = osPriorityAboveNormal,  /* > sensores: importa la latencia */
    };
    s_tid_alarma = osThreadNew(AlarmaThread, NULL, &attr);
    if (s_tid_alarma == NULL) return -3;

    return 0;
}

/* -------------------------------------------------------------------------- */
void Motor_Alarma_SetThreshold(float t_celsius)
{
    osMutexAcquire(s_mtx_state, osWaitForever);
    s_threshold_c = t_celsius;
    osMutexRelease(s_mtx_state);

    printf("[ALARMA] Nuevo umbral: %.2f C\r\n", t_celsius);
}

/* -------------------------------------------------------------------------- */
float Motor_Alarma_GetThreshold(void)
{
    float v;
    osMutexAcquire(s_mtx_state, osWaitForever);
    v = s_threshold_c;
    osMutexRelease(s_mtx_state);
    return v;
}

/* -------------------------------------------------------------------------- */
bool Motor_Alarma_IsActive(void)
{
    return s_active;
}

/* -------------------------------------------------------------------------- */
/* Llamado desde TID_TEMP tras cada lectura LM75A.
 * Histéresis: activa estricto a T > umbral, desactiva a T < (umbral - 1.0).
 */
void Motor_Alarma_OnNewTemperature(float t_celsius)
{
    float th;
    osMutexAcquire(s_mtx_state, osWaitForever);
    th = s_threshold_c;
    osMutexRelease(s_mtx_state);

    if (!s_active) {
        if (t_celsius > th) {
            printf("[ALARMA] T=%.2f > %.2f -> ACTIVAR\r\n", t_celsius, th);
            osThreadFlagsSet(s_tid_alarma, FLAG_ALARMA_ACTIVATE);
        }
    } else {
        if (t_celsius < (th - ALARMA_TEMP_HYSTERESIS_C)) {
            printf("[ALARMA] T=%.2f < %.2f -> DESACTIVAR\r\n",
                   t_celsius, th - ALARMA_TEMP_HYSTERESIS_C);
            osThreadFlagsSet(s_tid_alarma, FLAG_ALARMA_DEACTIVATE);
        }
    }
}

/* -------------------------------------------------------------------------- */
static void AlarmaPatternTimerCb(void *arg)
{
    (void)arg;
    osThreadFlagsSet(s_tid_alarma, FLAG_ALARMA_TICK);
}

/* -------------------------------------------------------------------------- */
static void AlarmaThread(void *arg)
{
    (void)arg;
    uint32_t freq = ALARMA_FREQ_LOW;
    int      dir  = +1;   /* +1 sube, -1 baja */

    while (1) {
        uint32_t flags = osThreadFlagsWait(
            FLAG_ALARMA_ACTIVATE | FLAG_ALARMA_DEACTIVATE | FLAG_ALARMA_TICK,
            osFlagsWaitAny, osWaitForever);

if (flags & FLAG_ALARMA_ACTIVATE) {
            if (!s_active) {
                s_active = true;
                freq = ALARMA_FREQ_LOW;
                dir  = +1;
                AlarmaPWM_SetFrequency(freq);
                AlarmaPWM_On();
                osTimerStart(s_tim_pattern, ALARMA_PATTERN_MS);

                /* === DEBUG: LED rojo (LD3) indica alarma activa === */
                LED_Encendido(2);

                /* TODO FASE FIBRA: notificar a NUCLEO-A
                 * Fiber_Send_AlarmaEvent(true, g_ultima_temperatura);
                 */
            }
        }

        if (flags & FLAG_ALARMA_DEACTIVATE) {
            if (s_active) {
                s_active = false;
                osTimerStop(s_tim_pattern);
                AlarmaPWM_Off();

                /* === DEBUG: LED rojo apagado === */
                LED_Apagado(2);

                /* TODO FASE FIBRA:
                 * Fiber_Send_AlarmaEvent(false, g_ultima_temperatura);
                 */
            }
        }

        if ((flags & FLAG_ALARMA_TICK) && s_active) {
            /* Barrido tipo sirena entre LOW y HIGH */
            freq += (uint32_t)((int32_t)ALARMA_FREQ_STEP * dir);
            if (freq >= ALARMA_FREQ_HIGH) { freq = ALARMA_FREQ_HIGH; dir = -1; }
            if (freq <= ALARMA_FREQ_LOW)  { freq = ALARMA_FREQ_LOW;  dir = +1; }
            AlarmaPWM_SetFrequency(freq);
        }
    }
}