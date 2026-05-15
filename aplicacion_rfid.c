/*------------------------------------------------------------------------------
 *  Aplicacion RFID - NUCLEO-B (Sentinel E)
 *
 *  ESTADO ACTUAL: FASE C - sincronizado con LPWR_Mode mediante flags
 *                          (sin Stop Mode real todavia, simulado con osDelay)
 *
 *  HARDWARE (SPI5 - mismo que en pruebas de NUCLEO-A):
 *      SCK  = PF7   MISO = PF8   MOSI = PF9
 *      CS   = PF13  RST  = PF14
 *
 *  FLUJO DE LA TAREA EN FASE C:
 *    1. Init: detectar chip una vez, despues bloquear esperando flag 0x40
 *    2. Recibe flag 0x40 (de LPWR) -> abre ventana de 5s buscando tarjeta
 *    3. Si lee tarjeta: printf + actualiza globales + rompe la ventana
 *    4. Al terminar la ventana, manda flag 0x81 a LPWR
 *
 *  FASES PREVISTAS:
 *    - FASE A: polling continuo (ya hecho)
 *    - FASE B: TEMP + RFID en paralelo (ya hecho)
 *    - FASE C (actual): boton/timer activan ventanas de 5s
 *    - FASE D: Stop Mode real entre ventanas
 *----------------------------------------------------------------------------*/

#include "aplicacion_rfid.h"
#include "cmsis_os2.h"
#include "mfrc522.h"
#include <stdio.h>
#include <string.h>
#include "fibra.h"

/* =========================================================
 *  SELECCION DE HARDWARE
 *  Para migrar al SPI3 final, comentar SPI5 y descomentar SPI3.
 * ========================================================= */
#define RFID_USA_SPI5
/* #define RFID_USA_SPI3 */

#if defined(RFID_USA_SPI5)

    #define RFID_SPI_INSTANCE       SPI5
    #define RFID_SPI_AF             GPIO_AF5_SPI5
    #define RFID_SPI_CLK_ENABLE()   __HAL_RCC_SPI5_CLK_ENABLE()
    #define RFID_SPI_PRESCALER      SPI_BAUDRATEPRESCALER_256

    #define RFID_SPI_GPIO_PORT      GPIOF
    #define RFID_SPI_GPIO_CLK_EN()  __HAL_RCC_GPIOF_CLK_ENABLE()
    #define RFID_SCK_PIN            GPIO_PIN_7
    #define RFID_MISO_PIN           GPIO_PIN_8
    #define RFID_MOSI_PIN           GPIO_PIN_9

    #define RFID_CS_RST_PORT        GPIOF
    #define RFID_CS_RST_CLK_EN()    __HAL_RCC_GPIOF_CLK_ENABLE()
    #define RFID_CS_PIN             GPIO_PIN_13
    #define RFID_RST_PIN            GPIO_PIN_14

#elif defined(RFID_USA_SPI3)

    #define RFID_SPI_INSTANCE       SPI3
    #define RFID_SPI_AF             GPIO_AF6_SPI3
    #define RFID_SPI_CLK_ENABLE()   __HAL_RCC_SPI3_CLK_ENABLE()
    #define RFID_SPI_PRESCALER      SPI_BAUDRATEPRESCALER_128

    #define RFID_SPI_GPIO_PORT      GPIOC
    #define RFID_SPI_GPIO_CLK_EN()  __HAL_RCC_GPIOC_CLK_ENABLE()
    #define RFID_SCK_PIN            GPIO_PIN_10
    #define RFID_MISO_PIN           GPIO_PIN_11
    #define RFID_MOSI_PIN           GPIO_PIN_12

    #define RFID_CS_RST_PORT        GPIOD
    #define RFID_CS_RST_CLK_EN()    __HAL_RCC_GPIOD_CLK_ENABLE()
    #define RFID_CS_PIN             GPIO_PIN_2
    #define RFID_RST_PIN            GPIO_PIN_3

#else
    #error "Debes definir RFID_USA_SPI5 o RFID_USA_SPI3"
#endif


/* =========================================================
 *  Parametros de la tarea
 * ========================================================= */
#define RFID_STACK_SIZE              2048U
#define RFID_DELAY_INICIAL_MS        2000U   /* esperar a que estabilice la alimentacion */
#define RFID_POLL_PERIOD_MS           200U   /* periodo de polling dentro de la ventana */
#define RFID_REINTENTO_DETECCION_MS  5000U   /* si no detecta el chip al inicio */

/* Ventana de lectura tras recibir flag de LPWR.
   25 iteraciones x 200 ms = 5000 ms (5s) - igual que la duracion de TEMP */
#define RFID_VENTANA_ITERACIONES      25U

/* Flags de sincronizacion con LPWR_Mode
   TODO: cuando se limpie la convencion de flags, cambiar 0x81 por 0x100
         para que TEMP (0x80) y RFID no compartan bits. */
#define RFID_FLAG_DESPERTAR          0x40U
#define RFID_FLAG_TERMINADO          0x100U


/* =========================================================
 *  Variables del modulo
 * ========================================================= */
SPI_HandleTypeDef hspi_rfid;
static MFRC522_HandleTypeDef lector_rfid;
osThreadId_t id_tarea_rfid = NULL;
static volatile uint8_t version_mfrc522 = 0U;
static volatile int chip_detectado = 0;

/* UID de la ultima tarjeta leida. Accesible desde watches del debugger */
uint8_t ultimo_uid_rfid[4] = {0, 0, 0, 0};

/* Contador de lecturas validas - util para watch */
volatile uint32_t g_rfid_total_lecturas = 0;

/* Handle del hilo LPWR para devolverle la flag al terminar la ventana */
extern osThreadId_t TID_LPWR;

static void Tarea_RFID(void *argumento);
static void MX_RFID_SPI_Init(void);
static void MX_RFID_GPIO_Init(void);
static int  Intentar_Leer_Tarjeta(uint8_t uid_salida[4]);
static int  Intentar_Detectar_Chip(void);


/* =========================================================
 *  INICIALIZACION DEL HARDWARE
 * ========================================================= */
static void MX_RFID_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    RFID_SPI_GPIO_CLK_EN();
    RFID_CS_RST_CLK_EN();

    /* CS y RST en alto antes de configurarlos como salida */
    HAL_GPIO_WritePin(RFID_CS_RST_PORT, RFID_CS_PIN | RFID_RST_PIN, GPIO_PIN_SET);

    /* SCK / MISO / MOSI en alternate function */
    GPIO_InitStruct.Pin       = RFID_SCK_PIN | RFID_MISO_PIN | RFID_MOSI_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = RFID_SPI_AF;
    HAL_GPIO_Init(RFID_SPI_GPIO_PORT, &GPIO_InitStruct);

    /* CS y RST como salida push-pull con pull-up */
    GPIO_InitStruct.Pin   = RFID_CS_PIN | RFID_RST_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RFID_CS_RST_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(RFID_CS_RST_PORT, RFID_CS_PIN | RFID_RST_PIN, GPIO_PIN_SET);
}

static void MX_RFID_SPI_Init(void)
{
    RFID_SPI_CLK_ENABLE();

    hspi_rfid.Instance               = RFID_SPI_INSTANCE;
    hspi_rfid.Init.Mode              = SPI_MODE_MASTER;
    hspi_rfid.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi_rfid.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi_rfid.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi_rfid.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi_rfid.Init.NSS               = SPI_NSS_SOFT;
    hspi_rfid.Init.BaudRatePrescaler = RFID_SPI_PRESCALER;
    hspi_rfid.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi_rfid.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi_rfid.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi_rfid.Init.CRCPolynomial     = 10;

    HAL_SPI_Init(&hspi_rfid);
}


/* =========================================================
 *  DETECCION DEL CHIP (3 lecturas consistentes de VersionReg)
 * ========================================================= */
static int Intentar_Detectar_Chip(void)
{
    uint8_t v1, v2, v3;

    if (MFRC522_Init(&lector_rfid) != HAL_OK)
    {
        return 0;
    }

    v1 = MFRC522_GetVersion(&lector_rfid);
    v2 = MFRC522_GetVersion(&lector_rfid);
    v3 = MFRC522_GetVersion(&lector_rfid);

    if ((v1 == v2) && (v2 == v3) &&
        ((v1 == 0x91U) || (v1 == 0x92U)))
    {
        version_mfrc522 = v1;
        return 1;
    }

    printf("RFID: lecturas inconsistentes 0x%02X 0x%02X 0x%02X\r\n",
           v1, v2, v3);
    return 0;
}


/* =========================================================
 *  API DE LA APLICACION
 * ========================================================= */
void Aplicacion_RFID_Inicializar(void)
{
    /* Solo configura el hardware, la deteccion del chip se hace en la tarea */
    MX_RFID_GPIO_Init();
    MX_RFID_SPI_Init();

    lector_rfid.hspi     = &hspi_rfid;
    lector_rfid.cs_port  = RFID_CS_RST_PORT;
    lector_rfid.cs_pin   = RFID_CS_PIN;
    lector_rfid.rst_port = RFID_CS_RST_PORT;
    lector_rfid.rst_pin  = RFID_RST_PIN;

    printf("RFID: hardware configurado (SPI5), deteccion diferida a la tarea\r\n");
}

void Aplicacion_RFID_Crear_Tarea(void)
{
    static const osThreadAttr_t atributos_tarea_rfid = {
        .name       = "TareaRFID",
        .priority   = osPriorityNormal,
        .stack_size = RFID_STACK_SIZE
    };

    id_tarea_rfid = osThreadNew(Tarea_RFID, NULL, &atributos_tarea_rfid);

    if (id_tarea_rfid == NULL)
    {
        printf("RFID ERROR: no se pudo crear la tarea\r\n");
    }
}


/* =========================================================
 *  INTENTAR LEER UNA TARJETA (1 ciclo de polling)
 * ========================================================= */
static int Intentar_Leer_Tarjeta(uint8_t uid_salida[4])
{
    uint8_t atqa[2];
    uint8_t longitud_atqa = 2U;

    if (MFRC522_RequestA(&lector_rfid, atqa, &longitud_atqa) != MFRC522_OK)
    {
        return 0;   /* no hay tarjeta en el campo */
    }

    if (MFRC522_ReadUid4(&lector_rfid, uid_salida) != MFRC522_OK)
    {
        return 0;   /* hay tarjeta pero la lectura del UID fallo */
    }

    return 1;
}


/* =========================================================
 *  TAREA RTOS - FASE C: ventana de 5s controlada por LPWR
 *  ---------------------------------------------------------
 *  Init: detectar chip una vez (con reintentos).
 *  Bucle: bloquear esperando 0x40, abrir ventana de 5s,
 *         si lee tarjeta -> printf + globals, terminar ventana,
 *         devolver 0x81 a LPWR.
 *
 *  === FASE A === (codigo original conservado, comentado)
 *  La version anterior hacia polling continuo cada 200ms sin
 *  bloquearse nunca y mandaba un "estoy vivo" cada 5s. Era util
 *  para validar el hardware sin necesidad de orquestador.
 * ========================================================= */
static void Tarea_RFID(void *argumento)
{
    (void)argumento;

    uint8_t  uid[4];
    uint8_t  uid_anterior_ventana[4];
    int      tarjeta_leida_en_ventana;
    uint32_t i;

    /* Esperar al arranque para que la alimentacion del MFRC522 estabilice */
    printf("RFID: esperando %lu ms antes de detectar el chip...\r\n",
           (unsigned long)RFID_DELAY_INICIAL_MS);
    osDelay(RFID_DELAY_INICIAL_MS);

    /* Detectar el chip (con reintentos infinitos) */
    while (!chip_detectado)
    {
        printf("RFID: intentando detectar MFRC522...\r\n");

        if (Intentar_Detectar_Chip())
        {
            printf("RFID OK: MFRC522 detectado (VersionReg=0x%02X)\r\n",
                   version_mfrc522);
            chip_detectado = 1;
            break;
        }

        printf("RFID: deteccion fallida, reintentar en %lu ms\r\n",
               (unsigned long)RFID_REINTENTO_DETECCION_MS);
        osDelay(RFID_REINTENTO_DETECCION_MS);
    }

printf("RFID: tarea lista, esperando flag de LPWR para abrir ventana\r\n");

    /* Bucle principal: esperar flag de LPWR, abrir ventana de 5s */
    for (;;)
    {
        /* 1. Bloquear hasta que LPWR nos despierte */
        osThreadFlagsWait(RFID_FLAG_DESPERTAR, osFlagsWaitAny, osWaitForever);

        printf("RFID: ventana abierta, acerque tarjeta (5s)...\r\n");

        tarjeta_leida_en_ventana = 0;
        memset(uid_anterior_ventana, 0, 4);

        /* 2. Ventana de lectura de 5s */
        for (i = 0; i < RFID_VENTANA_ITERACIONES; i++)
        {
					if (Intentar_Leer_Tarjeta(uid))
					{
							if (memcmp(uid, uid_anterior_ventana, 4) != 0)
							{
									/* === FASE FIBRA === Buffer de 64 bytes obligatorio:
										 la cola Tx de fibra tiene msg_size=64 y copia 64 bytes
										 desde el puntero. Con un buffer menor lee fuera. */
									char trama_fibra[64] = {0};

									memcpy(ultimo_uid_rfid,      uid, 4);
									memcpy(uid_anterior_ventana, uid, 4);
									g_rfid_total_lecturas++;

									printf("RFID: tarjeta UID=%02X:%02X:%02X:%02X (lectura #%lu)\r\n",
												 uid[0], uid[1], uid[2], uid[3],
												 (unsigned long)g_rfid_total_lecturas);

									/* === FASE FIBRA === Enviar UID a NUCLEO-A.
										 Formato: "U:AABBCCDD\n" (8 hex sin separadores, terminador LF) */
									snprintf(trama_fibra, sizeof(trama_fibra),
													 "U:%02X%02X%02X%02X\n",
													 uid[0], uid[1], uid[2], uid[3]);
									Fiber_Send(trama_fibra);
									printf("[B] TX fibra: %s", trama_fibra);

									tarjeta_leida_en_ventana = 1;
									break;
							}
					}
            osDelay(RFID_POLL_PERIOD_MS);
        }

        if (!tarjeta_leida_en_ventana)
        {
            printf("RFID: ventana cerrada sin tarjeta\r\n");
        }

        /* 3. Limpiar flags espurias acumuladas durante la ventana.
           Si LPWR mando otro RFID_FLAG_DESPERTAR mientras estabamos
           leyendo (no deberia pasar, pero por seguridad) lo descartamos. */
        osThreadFlagsClear(RFID_FLAG_DESPERTAR);

        /* 4. Avisar a LPWR de que hemos terminado */
        osThreadFlagsSet(TID_LPWR, RFID_FLAG_TERMINADO);
    }
	}