/*------------------------------------------------------------------------------
 *  Aplicacion RFID - lectura de tarjetas MIFARE y registro en historial
 *  Proyecto Sentinel E - ISE 2025/2026
 *
 *  HARDWARE
 *  --------
 *  En NUCLEO-A (pruebas actuales en SPI5):
 *      SCK  = PF7   MISO = PF8   MOSI = PF9
 *      CS   = PF13  RST  = PF14
 *
 *  En NUCLEO-B (diseno final en SPI3):
 *      SCK  = PC10  MISO = PC11  MOSI = PC12
 *      CS   = PD2   RST  = PD3
 *
 *  ESTA VERSION (depuracion - hardware con ruido):
 *    - El init solo configura GPIO y SPI. NO lee el chip.
 *    - La deteccion del chip se hace dentro de la tarea, tras un
 *      delay inicial de 2 segundos para dar tiempo a que la
 *      alimentacion se estabilice.
 *    - Reintentos infinitos de deteccion: si falla una vez, sigue
 *      probando cada 5 segundos. Asi si el chip se recupera tras
 *      el arranque, la tarea lo detectara.
 *    - Polling continuo a 200 ms cuando ya esta detectado.
 *    - SPI a velocidad muy baja (~351 kHz).
 *----------------------------------------------------------------------------*/

#include "aplicacion_rfid.h"
#include "cmsis_os2.h"
#include "mfrc522.h"
#include "historial.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* =========================================================
 *  SELECCION DE HARDWARE
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
#define RFID_STACK_SIZE          2048U
#define RFID_DELAY_INICIAL_MS    2000U   /* esperar al arranque */
#define RFID_POLL_PERIOD_MS       200U
#define RFID_DIAG_PERIOD_MS      5000U
#define RFID_REINTENTO_DETECCION_MS  5000U

/* =============================================================
 *  Estado de desbloqueo de la web por tarjeta master
 * =============================================================
 *  flag_acceso_maestro: true mientras la web permite editar
 *                       la configuracion (durante 120 s).
 *  hora_desbloqueo_ms : tick en el que se pulso la master.
 *  TIEMPO_BLOQUEO_MS  : duracion del desbloqueo. */
//#define TIEMPO_BLOQUEO_MS  120000U     /* 2 minutos */

//volatile bool     flag_acceso_maestro = false;
//static   uint32_t hora_desbloqueo_ms  = 0;

/* =========================================================
 *  Variables del modulo
 * ========================================================= */
SPI_HandleTypeDef hspi_rfid;
static MFRC522_HandleTypeDef lector_rfid;
osThreadId_t id_tarea_rfid = NULL;
static volatile uint8_t version_mfrc522 = 0U;
static volatile int chip_detectado = 0;

uint8_t ultimo_uid_rfid[4] = {0, 0, 0, 0};

extern char aShowTime[80];
extern char aShowDate[80];

static void Tarea_RFID(void *argumento);
static void MX_RFID_SPI_Init(void);
static void MX_RFID_GPIO_Init(void);
static void Registrar_UID_En_Historial(const uint8_t uid[4]);
static int  Intentar_Leer_Tarjeta(uint8_t uid_salida[4]);
static int  Intentar_Detectar_Chip(void);


/* =========================================================
 *  INICIALIZACION DEL HARDWARE (solo configura, no comunica)
 * ========================================================= */

static void MX_RFID_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    RFID_SPI_GPIO_CLK_EN();
    RFID_CS_RST_CLK_EN();

    HAL_GPIO_WritePin(RFID_CS_RST_PORT, RFID_CS_PIN | RFID_RST_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin       = RFID_SCK_PIN | RFID_MISO_PIN | RFID_MOSI_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = RFID_SPI_AF;
    HAL_GPIO_Init(RFID_SPI_GPIO_PORT, &GPIO_InitStruct);

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
 *  DETECCION DEL CHIP (llamada desde la tarea, no desde init)
 * ========================================================= */
static int Intentar_Detectar_Chip(void)
{
    uint8_t v1, v2, v3;

    if (MFRC522_Init(&lector_rfid) != HAL_OK)
    {
        return 0;
    }

    /* Leer VersionReg 3 veces seguidas: solo damos por bueno
       el chip si las 3 lecturas coinciden y son 0x91 o 0x92.
       Esto descarta lecturas espurias por ruido. */
    v1 = MFRC522_GetVersion(&lector_rfid);
    v2 = MFRC522_GetVersion(&lector_rfid);
    v3 = MFRC522_GetVersion(&lector_rfid);

    if ((v1 == v2) && (v2 == v3) &&
        ((v1 == 0x91U) || (v1 == 0x92U)))
    {
        version_mfrc522 = v1;
        return 1;
    }

    /* Para diagnostico: imprimir las 3 lecturas */
    printf("RFID: lecturas inconsistentes 0x%02X 0x%02X 0x%02X\r\n",
           v1, v2, v3);

    return 0;
}


/* =========================================================
 *  API DE LA APLICACION
 * ========================================================= */

void Aplicacion_RFID_Inicializar(void)
{
    /* Solo configura el hardware, no comunica con el chip.
       Esto evita resets en el arranque por consumo del MFRC522. */
    MX_RFID_GPIO_Init();
    MX_RFID_SPI_Init();

    lector_rfid.hspi     = &hspi_rfid;
    lector_rfid.cs_port  = RFID_CS_RST_PORT;
    lector_rfid.cs_pin   = RFID_CS_PIN;
    lector_rfid.rst_port = RFID_CS_RST_PORT;
    lector_rfid.rst_pin  = RFID_RST_PIN;

    printf("RFID: hardware configurado, deteccion diferida a la tarea\r\n");
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
 *  REGISTRO DEL UID EN EL HISTORIAL
 * ========================================================= */
static void Registrar_UID_En_Historial(const uint8_t uid[4])
{
    char timestamp[40];
    char descripcion[40];

    snprintf(timestamp, sizeof(timestamp), "%s %s",
             aShowTime, aShowDate);

    snprintf(descripcion, sizeof(descripcion),
             "UID=%02X:%02X:%02X:%02X",
             uid[0], uid[1], uid[2], uid[3]);

    Guardar_En_Historial("ACCESO", timestamp, descripcion);
}


/* =========================================================
 *  INTENTAR LEER UNA TARJETA
 * ========================================================= */
static int Intentar_Leer_Tarjeta(uint8_t uid_salida[4])
{
    uint8_t atqa[2];
    uint8_t longitud_atqa = 2U;

    if (MFRC522_RequestA(&lector_rfid, atqa, &longitud_atqa) != MFRC522_OK)
    {
        return 0;
    }

    if (MFRC522_ReadUid4(&lector_rfid, uid_salida) != MFRC522_OK)
    {
        return 0;
    }

    return 1;
}


/* =========================================================
 *  TAREA RTOS
 * =========================================================
 *  Fase 1: esperar 2 s para que la alimentacion se estabilice.
 *  Fase 2: bucle infinito de deteccion del chip cada 5 s hasta
 *          que responda con VersionReg consistente.
 *  Fase 3: bucle de polling continuo cada 200 ms buscando tarjeta.
 *          Si en algun momento Intentar_Leer falla 50 veces seguidas,
 *          asumimos que el chip se ha desconectado y volvemos a Fase 2.
 * ========================================================= */
static void Tarea_RFID(void *argumento)
{
    (void)argumento;

    uint8_t  uid[4];
    uint8_t  uid_anterior[4]   = {0, 0, 0, 0};
    int      tarjeta_presente  = 0;
    uint32_t t_ultima_diag     = 0U;
    uint32_t intentos_sin_leer = 0U;

    /* FASE 1: esperar al arranque */
    printf("RFID: esperando %lu ms antes de detectar el chip...\r\n",
           (unsigned long)RFID_DELAY_INICIAL_MS);
    osDelay(RFID_DELAY_INICIAL_MS);

    /* FASE 2: detectar el chip con reintentos infinitos */
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

    /* FASE 3: bucle de polling */
    printf("RFID: tarea de lectura activa, acerque tarjeta\r\n");

    for (;;)
    {
        if (Intentar_Leer_Tarjeta(uid))
        {
            int es_nueva = (memcmp(uid, uid_anterior, 4) != 0);

            if (es_nueva || !tarjeta_presente)
            {
                printf("RFID: tarjeta UID=%02X:%02X:%02X:%02X\r\n",
                       uid[0], uid[1], uid[2], uid[3]);

                memcpy(ultimo_uid_rfid, uid, 4);
                memcpy(uid_anterior,    uid, 4);

                Registrar_UID_En_Historial(uid);
            }

            tarjeta_presente = 1;
            intentos_sin_leer = 0;
        }
        else
        {
            intentos_sin_leer++;
            if (intentos_sin_leer > 5U)
            {
                tarjeta_presente = 0;
            }
        }

        /* Mensaje "estoy vivo" cada 5 segundos */
        uint32_t ahora = osKernelGetTickCount();
        if ((ahora - t_ultima_diag) >= RFID_DIAG_PERIOD_MS)
        {
            t_ultima_diag = ahora;
            if (!tarjeta_presente)
            {
                printf("RFID: esperando tarjeta...\r\n");
            }
        }

        osDelay(RFID_POLL_PERIOD_MS);
    }
}