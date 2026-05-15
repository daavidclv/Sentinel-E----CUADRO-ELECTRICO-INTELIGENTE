#ifndef __CONFIGURACION_H
#define __CONFIGURACION_H

#include <stdint.h>

/* =============================================================
 *  Modulo de configuracion persistente del sistema Sentinel E
 * =============================================================
 *  Almacena los parametros del sistema en el sector 0 de la
 *  Flash SPI externa (W25Q128 / GD25Q128).
 *
 *  Mapa de memoria:
 *    0x000000 - 0x000FFF : SECTOR DE CONFIGURACION (4 KiB)
 *    0x001000 en adelante : Historial de eventos
 *
 *  Los parametros se cargan al arrancar. Si la Flash esta vacia
 *  o corrupta, se cargan los valores por defecto.
 *
 *  Cualquier hilo del sistema puede consultar/modificar estas
 *  variables globales directamente. Para hacer cambios persistentes
 *  hay que llamar a Config_Guardar() despues de modificar.
 * ============================================================= */

#define CONFIG_FLASH_ADDR    0x000000U
#define CONFIG_MAGIC         0xC0FFEE00U   /* Marca de "configuracion valida" */

/* Longitudes maximas de los textos */
#define CONFIG_ID_LEN        32
#define CONFIG_UBIC_LEN      32
/* =========================================================
 *  Lista blanca de UIDs autorizados como master
 * ========================================================= */
#define NUM_UIDS_AUTORIZADOS  2

extern const uint8_t UIDS_AUTORIZADOS[NUM_UIDS_AUTORIZADOS][4];

/* Devuelve 1 si el UID esta en la lista blanca, 0 si no. */
int Config_UID_Es_Autorizado(const uint8_t uid[4]);
/* ---- Variables globales de configuracion ---- */
/* Definidas en configuracion.c, accesibles desde todo el proyecto */

extern char     g_id_dispositivo[CONFIG_ID_LEN];   /* "NUCLEO-A" */
extern char     g_ubicacion[CONFIG_UBIC_LEN];      /* "CUADRO_PRINCIPAL" */
extern uint16_t g_tasa_refresco_seg;               /* 5 */
extern uint16_t g_umbral_temp_c;                   /* 45 (grados Celsius) */
extern uint16_t g_umbral_corriente_dA;             /* 150 (decaamperios = 15.0 A) */
extern uint8_t  g_uid_master[4];                   /* {0xA1, 0xB2, 0xC3, 0xD4} */

/* ---- API publica ---- */

/**
 * @brief Carga la configuracion desde la Flash a las variables globales.
 *        Si la Flash esta vacia o corrupta, carga valores por defecto.
 *        Llamar una vez al arranque, despues de Memoria_Init().
 * @return 0 si cargo de Flash, 1 si uso defaults.
 */
int Config_Cargar(void);

/**
 * @brief Guarda las variables globales actuales en la Flash.
 *        Borra el sector 0 y escribe los valores actuales.
 *        Llamar tras modificar cualquier variable que deba persistir.
 * @return 0 si OK, -1 si error.
 */
int Config_Guardar(void);

/**
 * @brief Pone las variables globales a sus valores por defecto (en RAM).
 *        No graba en Flash. Llamar Config_Guardar() despues si quieres.
 */
void Config_PorDefecto(void);

#endif /* __CONFIGURACION_H */