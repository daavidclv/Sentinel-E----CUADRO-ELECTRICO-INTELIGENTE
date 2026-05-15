/*------------------------------------------------------------------------------
 *  Modulo de control de acceso maestro (desbloqueo de la web por RFID)
 *  Proyecto Sentinel E - ISE 2025/2026
 *
 *  Funcionamiento:
 *    - La fibra recibe UIDs leidos por NUCLEO-B
 *    - Si el UID esta en la lista blanca, se activa flag_acceso_maestro
 *      durante TIEMPO_BLOQUEO_MS (120 s)
 *    - Una tarea watchdog vigila el tiempo y bloquea de nuevo al expirar
 *----------------------------------------------------------------------------*/
#ifndef ACCESO_MAESTRO_H
#define ACCESO_MAESTRO_H

#include <stdint.h>
#include <stdbool.h>

/* Duracion del desbloqueo tras pasar tarjeta autorizada */
#define TIEMPO_BLOQUEO_MS   120000U   /* 2 minutos */

/* Estado de desbloqueo. Lo lee el CGI directamente. */
extern volatile bool flag_acceso_maestro;

/* Inicializa el modulo y arranca la tarea watchdog */
void Acceso_Maestro_Init(void);

/* Procesa un UID recibido por fibra. Devuelve 1 si era autorizado, 0 si no. */
int  Acceso_Maestro_Procesar_UID(const uint8_t uid[4]);

/* Devuelve los segundos que faltan para que se bloquee de nuevo (0 si bloqueado) */
uint32_t Acceso_Maestro_Segundos_Restantes(void);

#endif