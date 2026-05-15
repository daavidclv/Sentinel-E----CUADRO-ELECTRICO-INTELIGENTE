/*------------------------------------------------------------------------------
 *  Modulo de historial de eventos persistente
 *  Proyecto Sentinel E - ISE 2025/2026
 *
 *  Cambios respecto a la version anterior:
 *    - Offset HISTORIAL_INICIO (0x001000) para no pisar la configuracion
 *    - Variable global g_num_eventos para iterar al reves desde la web
 *    - Funcion Historial_Borrar_Todo() para el boton de la web
 *----------------------------------------------------------------------------*/

#include "historial.h"
#include "memoria_externa.h"
#include <stdio.h>

/* Identificadores RTOS */
static osThreadId_t      tid_Thread_Historial;
static osMessageQueueId_t colaHistorial;

/* Numero de eventos validos en la Flash. Accesible desde el CGI para
   iterar de mas reciente a mas antiguo. */
static uint32_t g_num_eventos = 0;

/* Prototipos locales */
static void     Thread_Historial(void *argument);
static int      Pagina_Esta_Libre(uint32_t direccion);
static uint32_t Buscar_Siguiente_Libre(void);

/* ------------------------------------------------------------------
 *  Init_Thread_Historial
 * ------------------------------------------------------------------ */
int Init_Thread_Historial(void) {
    colaHistorial = osMessageQueueNew(10, TAMANO_MENSAJE, NULL);
    if (colaHistorial == NULL) return -1;

    tid_Thread_Historial = osThreadNew(Thread_Historial, NULL, NULL);
    if (tid_Thread_Historial == NULL) return -1;

    return 0;
}

/* ------------------------------------------------------------------
 *  Guardar_En_Historial
 * ------------------------------------------------------------------ */
void Guardar_En_Historial(const char* tipo_evento,
                          const char* timestamp,
                          const char* descripcion) {
    char mensaje[TAMANO_MENSAJE];

    snprintf(mensaje, TAMANO_MENSAJE, "%s | %s | %s",
             tipo_evento, timestamp, descripcion);

    osMessageQueuePut(colaHistorial, mensaje, 0U, 0U);
}

/* ------------------------------------------------------------------
 *  Leer_De_Historial
 *  CORREGIDO: aplica el offset HISTORIAL_INICIO. El indice 0 corresponde
 *  al primer evento real, no a la direccion 0x000000 (configuracion).
 * ------------------------------------------------------------------ */
void Leer_De_Historial(uint32_t indice, char* salida) {
    uint32_t direccion = HISTORIAL_INICIO + (indice * TAMANO_MENSAJE);
    Memoria_ReadData(direccion, (uint8_t*)salida, TAMANO_MENSAJE);
}

/* ------------------------------------------------------------------
 *  Historial_Num_Eventos
 * ------------------------------------------------------------------ */
uint32_t Historial_Num_Eventos(void) {
    return g_num_eventos;
}

/* ------------------------------------------------------------------
 *  Historial_Borrar_Todo
 *  Borra todos los sectores que pueda contener eventos. Como no
 *  sabemos cuantos hay realmente, borramos un rango razonable.
 *  Para 33 eventos bastaria con borrar el sector 1 (4KiB = 64 eventos).
 *  Aqui borramos los primeros 16 sectores (64 KiB = 1024 eventos)
 *  para tener margen de sobra.
 * ------------------------------------------------------------------ */
void Historial_Borrar_Todo(void) {
    /* Borrar 16 sectores empezando desde HISTORIAL_INICIO */
    for (uint32_t s = 0; s < 16; s++) {
        Memoria_EraseSector(HISTORIAL_INICIO + (s * W25Q_SECTOR_SIZE));
    }

    /* Reiniciar el contador de eventos */
    g_num_eventos = 0;

    /* Aviso: si Thread_Historial estaba escribiendo en una direccion
       avanzada (porque ya tenia muchos eventos), su variable local
       'direccion' interna no se reinicia. La proxima vez que arranquemos
       el sistema (reset), Buscar_Siguiente_Libre() devolvera HISTORIAL_INICIO
       y todo vuelve a la normalidad. */
}

/* ------------------------------------------------------------------
 *  Pagina_Esta_Libre (interna)
 * ------------------------------------------------------------------ */
static int Pagina_Esta_Libre(uint32_t direccion) {
    uint8_t buffer[8];
    Memoria_ReadData(direccion, buffer, 8);
    for (int i = 0; i < 8; i++) {
        if (buffer[i] != 0xFF) return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------
 *  Buscar_Siguiente_Libre (interna)
 *  CORREGIDO: la busqueda binaria opera en el rango
 *  [PAGINA_INICIAL, TOTAL_PAGINAS), saltando el sector 0 (configuracion).
 * ------------------------------------------------------------------ */
static uint32_t Buscar_Siguiente_Libre(void) {
    const uint32_t TOTAL_PAGINAS = W25Q_TOTAL_SIZE / TAMANO_MENSAJE;
    const uint32_t PAGINA_INICIAL = HISTORIAL_INICIO / TAMANO_MENSAJE;

    uint32_t izquierda = PAGINA_INICIAL;
    uint32_t derecha   = TOTAL_PAGINAS - 1U;
    uint32_t primera_libre;

    /* Caso 1: el historial esta totalmente vacio */
    if (Pagina_Esta_Libre(HISTORIAL_INICIO)) {
        g_num_eventos = 0;
        return HISTORIAL_INICIO;
    }

    /* Caso 2: el historial esta totalmente lleno (poco probable: 16 MB) */
    if (!Pagina_Esta_Libre(derecha * TAMANO_MENSAJE)) {
        g_num_eventos = TOTAL_PAGINAS - PAGINA_INICIAL;
        return HISTORIAL_INICIO;   /* Politica circular: empezar de nuevo */
    }

    /* Busqueda binaria: invariante izquierda escrita, derecha libre */
    primera_libre = derecha;
    while (izquierda + 1U < derecha) {
        uint32_t medio = (izquierda + derecha) / 2U;
        if (Pagina_Esta_Libre(medio * TAMANO_MENSAJE)) {
            derecha = medio;
            primera_libre = medio;
        } else {
            izquierda = medio;
        }
    }

    /* Actualizar el contador: numero de paginas escritas desde el inicio */
    g_num_eventos = primera_libre - PAGINA_INICIAL;

    return primera_libre * TAMANO_MENSAJE;
}

/* ------------------------------------------------------------------
 *  Thread_Historial
 *  CORREGIDO: la politica circular vuelve a HISTORIAL_INICIO, no a 0.
 *             Incrementa g_num_eventos en cada escritura.
 * ------------------------------------------------------------------ */
static void Thread_Historial(void *argument) {
    (void)argument;
    char texto[TAMANO_MENSAJE];
    uint32_t direccion = Buscar_Siguiente_Libre();

    while (1) {
        osMessageQueueGet(colaHistorial, texto, NULL, osWaitForever);

        /* Inicio de sector nuevo: borrar antes de escribir */
        if ((direccion % W25Q_SECTOR_SIZE) == 0U) {
            Memoria_EraseSector(direccion);
        }

        Memoria_WritePage(direccion, (uint8_t*)texto, TAMANO_MENSAJE);

        /* Avanzar y actualizar el contador global */
        direccion += TAMANO_MENSAJE;
        g_num_eventos++;

        /* Politica circular al llegar al final del chip */
        if (direccion >= W25Q_TOTAL_SIZE) {
            direccion = HISTORIAL_INICIO;
        }
    }
}