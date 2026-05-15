/*------------------------------------------------------------------------------
 *  Gestion de configuracion persistente (sin estructuras de datos)
 *  Proyecto Sentinel E - ISE 2025/2026
 *
 *  Filosofia:
 *    - Cada parametro es una variable global suelta, facil de leer/modificar
 *    - Para guardar: copiamos las variables a un buffer y lo escribimos
 *    - Para cargar: leemos el buffer y descomponemos en las variables
 *
 *  Layout del buffer en Flash (256 bytes = 1 pagina):
 *    Offset  0 -  3 : MAGIC (4 bytes, uint32_t)
 *    Offset  4 - 35 : id_dispositivo (32 bytes char)
 *    Offset 36 - 67 : ubicacion (32 bytes char)
 *    Offset 68 - 69 : tasa_refresco_seg (2 bytes, uint16_t)
 *    Offset 70 - 71 : umbral_temp_c (2 bytes, uint16_t)
 *    Offset 72 - 73 : umbral_corriente_dA (2 bytes, uint16_t)
 *    Offset 74 - 77 : uid_master[4] (4 bytes)
 *    Offset 78 -255 : Reservado (relleno con 0x00)
 *----------------------------------------------------------------------------*/

#include "configuracion.h"
#include "memoria_externa.h"
#include <string.h>

/* ---- Variables globales (accesibles desde todo el proyecto) ---- */
char     g_id_dispositivo[CONFIG_ID_LEN];
char     g_ubicacion[CONFIG_UBIC_LEN];
uint16_t g_tasa_refresco_seg;
uint16_t g_umbral_temp_c;
uint16_t g_umbral_corriente_dA;
uint8_t  g_uid_master[4];

/* Buffer auxiliar de 256 bytes (1 pagina de Flash) */
static uint8_t buffer_flash[256];

/* Offsets dentro del buffer */
#define OFF_MAGIC     0
#define OFF_ID        4
#define OFF_UBIC      36
#define OFF_TASA      68
#define OFF_UMBR_T    70
#define OFF_UMBR_I    72
#define OFF_UID       74



/* ------------------------------------------------------------------
 *  Lista blanca de tarjetas autorizadas como master
 *  ------------------------------------------------------------------
 *  Solo los UIDs que aparezcan en este array se aceptaran como
 *  master desde la web. Para anadir tarjetas: leer su UID con la
 *  web, copiarlo aqui y aumentar NUM_UIDS_AUTORIZADOS en el .h.
 * ------------------------------------------------------------------ */
const uint8_t UIDS_AUTORIZADOS[NUM_UIDS_AUTORIZADOS][4] = {
    {0xA5, 0x3C, 0x2C, 0x1F},   /* Tarjeta 1 */
    {0x41, 0x8E, 0xE7, 0xA9}    /* Tarjeta 2 */
};

int Config_UID_Es_Autorizado(const uint8_t uid[4]) {
    int i;
    for (i = 0; i < NUM_UIDS_AUTORIZADOS; i++) {
        if (memcmp(uid, UIDS_AUTORIZADOS[i], 4) == 0) {
            return 1;
        }
    }
    return 0;
}
/* ------------------------------------------------------------------
 *  Config_PorDefecto
 *  Carga los valores por defecto en las variables globales (RAM).
 *  No toca la Flash.
 * ------------------------------------------------------------------ */
void Config_PorDefecto(void) {
    /* Texto: usar strncpy y forzar terminador nulo */
    memset(g_id_dispositivo, 0, CONFIG_ID_LEN);
    strncpy(g_id_dispositivo, "NUCLEO-A", CONFIG_ID_LEN - 1);

    memset(g_ubicacion, 0, CONFIG_UBIC_LEN);
    strncpy(g_ubicacion, "CUADRO_PRINCIPAL", CONFIG_UBIC_LEN - 1);

    /* Numeros */
    g_tasa_refresco_seg   = 5;
    g_umbral_temp_c       = 27;
    g_umbral_corriente_dA = 150;   /* = 15.0 A */

    /* UID Master por defecto */
    g_uid_master[0] = 0xA5;
    g_uid_master[1] = 0x3C;
    g_uid_master[2] = 0x2C;
    g_uid_master[3] = 0x1F;
}

/* ------------------------------------------------------------------
 *  Config_Cargar
 *  Lee 256 bytes de la Flash, comprueba el MAGIC y descompone los
 *  valores en las variables globales. Si no hay datos validos,
 *  carga los valores por defecto.
 * ------------------------------------------------------------------ */
int Config_Cargar(void) {
    /* 1. Leer 256 bytes desde la direccion de configuracion */
    Memoria_ReadData(CONFIG_FLASH_ADDR, buffer_flash, 256);

    /* 2. Comprobar el MAGIC (los primeros 4 bytes) */
    uint32_t magic_leido = ((uint32_t)buffer_flash[0])       |
                           ((uint32_t)buffer_flash[1] << 8)  |
                           ((uint32_t)buffer_flash[2] << 16) |
                           ((uint32_t)buffer_flash[3] << 24);

    if (magic_leido != CONFIG_MAGIC) {
        /* Flash vacia o sin configuracion valida -> defaults */
        Config_PorDefecto();
        return 1;
    }

    /* 3. MAGIC valido: descomponer el buffer en las variables */

    /* Texto: copiamos y forzamos terminador nulo por seguridad */
    memcpy(g_id_dispositivo, &buffer_flash[OFF_ID], CONFIG_ID_LEN);
    g_id_dispositivo[CONFIG_ID_LEN - 1] = '\0';

    memcpy(g_ubicacion, &buffer_flash[OFF_UBIC], CONFIG_UBIC_LEN);
    g_ubicacion[CONFIG_UBIC_LEN - 1] = '\0';

    /* uint16_t en little-endian: byte bajo + byte alto<<8 */
    g_tasa_refresco_seg   = buffer_flash[OFF_TASA]   | (buffer_flash[OFF_TASA + 1]   << 8);
    g_umbral_temp_c       = buffer_flash[OFF_UMBR_T] | (buffer_flash[OFF_UMBR_T + 1] << 8);
    g_umbral_corriente_dA = buffer_flash[OFF_UMBR_I] | (buffer_flash[OFF_UMBR_I + 1] << 8);

    /* UID Master: 4 bytes directos */
    g_uid_master[0] = buffer_flash[OFF_UID];
    g_uid_master[1] = buffer_flash[OFF_UID + 1];
    g_uid_master[2] = buffer_flash[OFF_UID + 2];
    g_uid_master[3] = buffer_flash[OFF_UID + 3];

    return 0;
}

/* ------------------------------------------------------------------
 *  Config_Guardar
 *  Empaqueta las variables globales en el buffer y lo escribe en Flash.
 *  Borra el sector 0 antes de escribir.
 * ------------------------------------------------------------------ */
int Config_Guardar(void) {
    /* 1. Limpiar el buffer */
    memset(buffer_flash, 0, 256);

    /* 2. Empaquetar el MAGIC (4 bytes, little-endian) */
    buffer_flash[0] =  CONFIG_MAGIC        & 0xFF;
    buffer_flash[1] = (CONFIG_MAGIC >> 8)  & 0xFF;
    buffer_flash[2] = (CONFIG_MAGIC >> 16) & 0xFF;
    buffer_flash[3] = (CONFIG_MAGIC >> 24) & 0xFF;

    /* 3. Empaquetar los textos */
    memcpy(&buffer_flash[OFF_ID],   g_id_dispositivo, CONFIG_ID_LEN);
    memcpy(&buffer_flash[OFF_UBIC], g_ubicacion,      CONFIG_UBIC_LEN);

    /* 4. Empaquetar los uint16_t (little-endian) */
    buffer_flash[OFF_TASA]       =  g_tasa_refresco_seg       & 0xFF;
    buffer_flash[OFF_TASA + 1]   = (g_tasa_refresco_seg >> 8) & 0xFF;

    buffer_flash[OFF_UMBR_T]     =  g_umbral_temp_c           & 0xFF;
    buffer_flash[OFF_UMBR_T + 1] = (g_umbral_temp_c >> 8)     & 0xFF;

    buffer_flash[OFF_UMBR_I]     =  g_umbral_corriente_dA     & 0xFF;
    buffer_flash[OFF_UMBR_I + 1] = (g_umbral_corriente_dA >> 8) & 0xFF;

    /* 5. Empaquetar el UID Master (4 bytes directos) */
    buffer_flash[OFF_UID]     = g_uid_master[0];
    buffer_flash[OFF_UID + 1] = g_uid_master[1];
    buffer_flash[OFF_UID + 2] = g_uid_master[2];
    buffer_flash[OFF_UID + 3] = g_uid_master[3];

    /* 6. Borrar el sector 0 (4 KiB, ~100-400 ms) */
    Memoria_EraseSector(CONFIG_FLASH_ADDR);

    /* 7. Escribir la pagina con todos los datos */
    Memoria_WritePage(CONFIG_FLASH_ADDR, buffer_flash, 256);

    /* 8. Verificacion: releer y comparar */
    uint8_t verificacion[256];
    Memoria_ReadData(CONFIG_FLASH_ADDR, verificacion, 256);

    if (memcmp(buffer_flash, verificacion, 256) != 0) {
        return -1;   /* Error: lo escrito no coincide con lo leido */
    }

    return 0;
}