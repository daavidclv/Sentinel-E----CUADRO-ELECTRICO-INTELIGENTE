#ifndef __HISTORIAL_H
#define __HISTORIAL_H

#include "cmsis_os2.h"
#include <stdint.h>

/* =============================================================
 *  Modulo de historial de eventos persistente
 * =============================================================
 *  Almacena eventos en la Flash SPI externa W25Q128 / GD25Q128.
 *  Cada evento ocupa una pagina de TAMANO_MENSAJE bytes (64).
 *  Los eventos persisten tras reset o perdida de alimentacion.
 *
 *  Mapa de memoria:
 *    0x000000 - 0x000FFF  : Configuracion del sistema (NO TOCAR)
 *    0x001000 en adelante : Eventos del historial
 * ============================================================= */

#define TAMANO_MENSAJE       64
#define HISTORIAL_INICIO     0x001000U   /* Sector 1, tras la configuracion */

/* ---- API publica ---- */

/**
 * @brief Crea la cola de mensajes y lanza el hilo de historial.
 *        Al arrancar, localiza la siguiente posicion libre en Flash
 *        para no sobrescribir eventos anteriores.
 * @return 0 si OK, -1 si error.
 */
int Init_Thread_Historial(void);

/**
 * @brief Encola un evento para ser grabado en Flash.
 * @param tipo_evento "INFO", "ALARMA", "ACCESO", ...
 * @param timestamp   "HH:MM:SS dd/mm/aa"
 * @param descripcion Descripcion breve del evento.
 */
void Guardar_En_Historial(const char* tipo_evento,
                          const char* timestamp,
                          const char* descripcion);

/**
 * @brief Lee un evento desde la Flash por su indice.
 * @param indice 0 = primero (mas antiguo), N-1 = ultimo (mas reciente).
 * @param salida Buffer de al menos TAMANO_MENSAJE bytes.
 * @note  Si la pagina esta libre, salida[0] valdra 0xFF.
 */
void Leer_De_Historial(uint32_t indice, char* salida);

/**
 * @brief Devuelve el numero de eventos almacenados actualmente.
 *        Util para iterar de mas reciente a mas antiguo.
 * @return Numero de eventos validos (0 si la Flash esta vacia).
 */
uint32_t Historial_Num_Eventos(void);

/**
 * @brief Borra TODO el historial (varios sectores). Operacion lenta:
 *        cada sector tarda hasta ~400 ms en borrarse.
 *        Conserva la configuracion (que esta en el sector 0).
 *        Tras borrar, el contador de eventos se reinicia a 0.
 */
void Historial_Borrar_Todo(void);

#endif /* __HISTORIAL_H */