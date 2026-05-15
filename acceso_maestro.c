/*------------------------------------------------------------------------------
 *  Implementacion del modulo de acceso maestro
 *  Proyecto Sentinel E - ISE 2025/2026
 *----------------------------------------------------------------------------*/
#include "acceso_maestro.h"
#include "configuracion.h"
#include "historial.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

/* Marca temporal mostrada en la web (definida en HTTP_Server.c) */
extern char aShowTime[80];

/* ---- Estado interno ---- */
volatile bool   flag_acceso_maestro = false;
static volatile uint32_t hora_desbloqueo_ms = 0U;

/* ---- Tarea watchdog ---- */
static osThreadId_t tid_watchdog = NULL;

static uint64_t watchdog_stk[512 / 8];
static const osThreadAttr_t watchdog_attr = {
    .name       = "AccMaestroWD",
    .stack_mem  = watchdog_stk,
    .stack_size = sizeof(watchdog_stk),
    .priority   = osPriorityLow
};

/* ------------------------------------------------------------------
 *  Tarea watchdog: comprueba cada 1 s si el desbloqueo ha expirado.
 * ------------------------------------------------------------------ */
static void Tarea_Watchdog(void *arg) {
    (void)arg;

    for (;;) {
        if (flag_acceso_maestro) {
            uint32_t ahora = osKernelGetTickCount();
            /* La resta funciona aunque el contador desborde (uint32_t) */
            if ((ahora - hora_desbloqueo_ms) >= TIEMPO_BLOQUEO_MS) {
                flag_acceso_maestro = false;
                Guardar_En_Historial("INFO", aShowTime,
                                     "Web bloqueada (timeout)");
                printf("[ACCESO] Bloqueo automatico por timeout\r\n");
            }
        }
        osDelay(1000);
    }
}

/* ------------------------------------------------------------------
 *  Acceso_Maestro_Init
 * ------------------------------------------------------------------ */
void Acceso_Maestro_Init(void) {
    flag_acceso_maestro = false;
    hora_desbloqueo_ms  = 0U;

    tid_watchdog = osThreadNew(Tarea_Watchdog, NULL, &watchdog_attr);
    if (tid_watchdog == NULL) {
        printf("[ACCESO] ERROR: no se pudo crear watchdog\r\n");
    } else {
        printf("[ACCESO] Modulo de acceso maestro inicializado\r\n");
    }
}

/* ------------------------------------------------------------------
 *  Acceso_Maestro_Procesar_UID
 *  Se llama desde Fiber_Rx_Thread al recibir "U:XXXXXXXX\n".
 * ------------------------------------------------------------------ */
int Acceso_Maestro_Procesar_UID(const uint8_t uid[4]) {
    char descripcion[40];

    snprintf(descripcion, sizeof(descripcion),
             "UID=%02X:%02X:%02X:%02X",
             uid[0], uid[1], uid[2], uid[3]);

    /* === Solo la tarjeta master ACTIVA desbloquea === */
    if (memcmp(uid, g_uid_master, 4) == 0) {
        flag_acceso_maestro = true;
        hora_desbloqueo_ms  = osKernelGetTickCount();

        Guardar_En_Historial("ACCESO", aShowTime, descripcion);
        printf("[ACCESO] Web DESBLOQUEADA por %s (%u s)\r\n",
               descripcion, TIEMPO_BLOQUEO_MS / 1000U);
        return 1;
    }

    /* UID conocido pero no es la master actual, o UID totalmente desconocido */
    Guardar_En_Historial("ALARMA", aShowTime, descripcion);
    printf("[ACCESO] UID NO es la master actual: %s\r\n", descripcion);
    return 0;
}

/* ------------------------------------------------------------------
 *  Acceso_Maestro_Segundos_Restantes
 *  Util para mostrar el tiempo restante en la web (opcional).
 * ------------------------------------------------------------------ */
uint32_t Acceso_Maestro_Segundos_Restantes(void) {
    if (!flag_acceso_maestro) return 0U;

    uint32_t ahora      = osKernelGetTickCount();
    uint32_t transcurr  = ahora - hora_desbloqueo_ms;

    if (transcurr >= TIEMPO_BLOQUEO_MS) return 0U;
    return (TIEMPO_BLOQUEO_MS - transcurr) / 1000U;
}