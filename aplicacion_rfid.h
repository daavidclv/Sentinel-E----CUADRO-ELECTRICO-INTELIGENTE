#ifndef APLICACION_RFID_H
#define APLICACION_RFID_H

#include "stm32f4xx_hal.h"

/* =============================================================
 *  Aplicacion RFID - NUCLEO-B (Sentinel E)
 *  En NUCLEO-B usamos los mismos pines de prueba (SPI5) hasta
 *  que se valide el modulo. Para migrar al SPI3 final, modificar
 *  el #define en aplicacion_rfid.c
 * ============================================================= */

extern uint8_t ultimo_uid_rfid[4];
extern volatile uint32_t g_rfid_total_lecturas;

void Aplicacion_RFID_Inicializar(void);
void Aplicacion_RFID_Crear_Tarea(void);

#endif