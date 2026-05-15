#ifndef APLICACION_RFID_H
#define APLICACION_RFID_H

#include "stm32f4xx_hal.h"

/* =============================================================
 *  Modulo de aplicacion RFID (MFRC522)
 *  Proyecto Sentinel E - ISE 2025/2026
 * =============================================================
 *  CONFIGURACION DE PINES Y SPI
 *  ---------------------------------------------------------------
 *  En NUCLEO-A (fase de pruebas) usamos SPI5 - puerto F completo:
 *      SCK  = PF7   MISO = PF8   MOSI = PF9
 *      CS   = PF13  RST  = PF14
 *  SPI3 esta ocupado por la memoria externa W25Q128.
 *
 *  En NUCLEO-B (diseno final) el RFID ira en SPI3:
 *      SCK  = PC10  MISO = PC11  MOSI = PC12
 *      CS   = PD2   RST  = PD3
 *
 *  Para cambiar entre ambas configuraciones, comenta una
 *  definicion y descomenta la otra en aplicacion_rfid.c.
 * ============================================================= */

extern uint8_t ultimo_uid_rfid[4];

void Aplicacion_RFID_Inicializar(void);
void Aplicacion_RFID_Crear_Tarea(void);
/* =========================================================
 *  Estado del desbloqueo por tarjeta master
 * =========================================================
 *  flag_acceso_maestro = true durante 120 s tras pasar la
 *  tarjeta master. Mientras esta a true, la web permite
 *  modificar la configuracion. Tras el timeout vuelve a false. */

#endif