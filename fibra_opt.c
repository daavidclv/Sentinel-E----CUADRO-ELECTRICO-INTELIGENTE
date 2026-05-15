#include "fibra_opt.h"
#include "cmsis_os2.h"
#include "Driver_USART.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

extern ARM_DRIVER_USART Driver_USART2;  // Driver CMSIS USART2 (fibra optica)

/* Cola de mensajes pendientes para enviar (hasta 10 tramas de 64 bytes) */
static osMessageQueueId_t colaTx;

/* Cola de bytes recibidos (hasta 64 bytes sin leer) */
static osMessageQueueId_t colaRx;

/* ID del hilo TX para que la ISR pueda desbloquearlo con osThreadFlagsSet */
static osThreadId_t tid_tx;

/* Buffer donde el driver deposita cada byte recibido por DMA/interrupcion.
   Solo tiene 1 byte porque relanzamos Receive(1) tras cada byte recibido */
static uint8_t rx_byte;

/* Stack dedicado para el hilo TX de fibra */
static uint64_t fiber_int_stk[512 / 8];
static const osThreadAttr_t fiber_int_attr = {
    .name = "fiber_int", .stack_mem = fiber_int_stk, .stack_size = sizeof(fiber_int_stk)
};

/* Contadores de errores UART. No se usan para la deteccion de cable
   (eso lo hace Fibra_Cable_Conectado via GPIO), pero se mantienen
   para trazabilidad y posible uso futuro */
volatile uint32_t g_fibra_errores    = 0;
volatile uint32_t g_ultimo_error_tick = 0;


/* ------------------------------------------------------------------
 *  Fiber_Callback
 *  ISR del driver CMSIS USART2. Se llama desde contexto de interrupcion.
 *  No puede bloquear ni llamar a funciones no-ISR-safe.
 *
 *  SEND_COMPLETE   -> desbloquea el hilo TX para que envie el siguiente
 *  RECEIVE_COMPLETE -> mete el byte en colaRx y relanza otra recepcion
 *  Errores UART    -> incrementa contador y relanza recepcion (no se detiene)
 * ------------------------------------------------------------------ */
static void Fiber_Callback(uint32_t event) {
    if (event & ARM_USART_EVENT_SEND_COMPLETE) {
        osThreadFlagsSet(tid_tx, 0x01);         // Desbloquea fiber_tx_task
    }
    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        osMessageQueuePut(colaRx, &rx_byte, 0, 0);  // Byte -> cola RX
        Driver_USART2.Receive(&rx_byte, 1);          // Prepara siguiente byte
    }
    if (event & (ARM_USART_EVENT_RX_FRAMING_ERROR |
                 ARM_USART_EVENT_RX_PARITY_ERROR  |
                 ARM_USART_EVENT_RX_OVERFLOW      |
                 ARM_USART_EVENT_RX_BREAK)) {
        g_fibra_errores++;
        g_ultimo_error_tick = osKernelGetTickCount();
        Driver_USART2.Receive(&rx_byte, 1);          // Recupera recepcion tras error
    }
}


/* ------------------------------------------------------------------
 *  fiber_tx_task
 *  Hilo TX: bloquea en la cola hasta que alguien llame a Fiber_Send,
 *  luego envia el mensaje por USART2 y espera el flag de fin de envio
 *  (lo pone la ISR en SEND_COMPLETE). Serializa todos los envios.
 * ------------------------------------------------------------------ */
static void fiber_tx_task(void *arg) {
    char mensaje[64];
    while (1) {
        // Bloquea hasta que haya un mensaje en la cola TX
        if (osMessageQueueGet(colaTx, &mensaje, NULL, osWaitForever) == osOK) {
            Driver_USART2.Send(mensaje, strlen(mensaje));
            // Espera confirmacion de la ISR antes de enviar el siguiente
            osThreadFlagsWait(0x01, osFlagsWaitAny, osWaitForever);
        }
    }
}


/* ------------------------------------------------------------------
 *  Fiber_Init
 *  Inicializa el modulo completo de fibra:
 *    1. Colas TX y RX
 *    2. Driver USART2 a 9600 bps, 8N1, sin flow control
 *    3. Reconfiguración manual de PD6 con pull-down (deteccion de cable)
 *    4. Lanza recepcion continua byte a byte
 *    5. Crea el hilo TX
 *
 *  Pines fisicos (USART2 en NUCLEO-F429ZI):
 *    PD5 -> USART2_TX -> transmisor optico HFBR-1414Z
 *    PD6 -> USART2_RX -> receptor optico HFBR-2412TZ
 * ------------------------------------------------------------------ */
void Fiber_Init(void) {
    GPIO_InitTypeDef gpio_rx;

    // Crea cola TX: hasta 10 mensajes de 64 bytes
    colaTx = osMessageQueueNew(10, 64, NULL);
    // Crea cola RX: hasta 64 bytes sin leer
    colaRx = osMessageQueueNew(64, sizeof(uint8_t), NULL);

    Driver_USART2.Initialize(Fiber_Callback);
    Driver_USART2.PowerControl(ARM_POWER_FULL);
    // 9600 bps, 8 bits, sin paridad, 1 stop bit
    Driver_USART2.Control(ARM_USART_MODE_ASYNCHRONOUS |
                          ARM_USART_DATA_BITS_8 |
                          ARM_USART_PARITY_NONE  |
                          ARM_USART_STOP_BITS_1  |
                          ARM_USART_FLOW_CONTROL_NONE, 9600);
    Driver_USART2.Control(ARM_USART_CONTROL_TX, 1);   // Habilita TX
    Driver_USART2.Control(ARM_USART_CONTROL_RX, 1);   // Habilita RX

    /* Reconfigura PD6 (USART2_RX) con pull-down interno.
       El driver CMSIS lo deja como AF sin pull.
       Con pull-down:
         - Cable conectado    -> TX del otro extremo mantiene linea en HIGH
         - Cable desconectado -> pull-down lleva linea a LOW
       Esto permite a Fibra_Cable_Conectado detectar el estado del cable. */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    gpio_rx.Pin       = GPIO_PIN_6;
    gpio_rx.Mode      = GPIO_MODE_AF_PP;
    gpio_rx.Pull      = GPIO_PULLDOWN;
    gpio_rx.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_rx.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOD, &gpio_rx);

    Driver_USART2.Receive(&rx_byte, 1);   // Arranca recepcion continua

    tid_tx = osThreadNew(fiber_tx_task, NULL, &fiber_int_attr);
}


/* ------------------------------------------------------------------
 *  Fiber_Send
 *  Encola un mensaje de texto para enviar por fibra.
 *  No bloquea: si la cola esta llena (10 mensajes), descarta
 *  silenciosamente (timeout=0). Segura para llamar desde cualquier
 *  contexto (CGI, hilo, etc.).
 * ------------------------------------------------------------------ */
void Fiber_Send(char *texto) {
    osMessageQueuePut(colaTx, texto, 0, 0);
}


/* ------------------------------------------------------------------
 *  Fiber_Read
 *  Extrae un byte de la cola RX. Bloquea hasta que llegue uno.
 *  La llama Fiber_Rx_Thread en main.c en su bucle principal.
 * ------------------------------------------------------------------ */
uint8_t Fiber_Read(void) {
    uint8_t b;
    if (osMessageQueueGet(colaRx, &b, NULL, osWaitForever) == osOK) {
        return b;
    }
    return 0;
}


/* ------------------------------------------------------------------
 *  Fibra_Cable_Conectado
 *  Detecta fisicamente si el cable de fibra esta conectado leyendo PD6.
 *
 *  Logica con pull-down:
 *    - UART idle = linea en HIGH (el TX del otro extremo la mantiene alta)
 *    - Cable desconectado = pull-down lleva la linea a LOW
 *
 *  Para no confundir un bit '0' de un byte en transmision con
 *  cable desconectado, toma 20 muestras cada ~5us (~100us total).
 *  A 9600 bps un bit dura ~104us, asi que la ventana cubre casi
 *  un bit completo. Si >=14 de 20 muestras son HIGH, hay cable.
 *
 *  Devuelve 1 si conectado, 0 si no.
 * ------------------------------------------------------------------ */
int Fibra_Cable_Conectado(void) {
    int cuenta_high = 0;
    int i;

    for (i = 0; i < 20; i++) {
        if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_6) == GPIO_PIN_SET) {
            cuenta_high++;
        }
        /* Delay de ~5us a 168MHz: 200 NOPs ~ 200 * 1/168MHz ~ 1.2us,
           mas overhead del bucle, total ~5us por iteracion */
        {
            volatile int j;
            for (j = 0; j < 200; j++) { __NOP(); }
        }
    }

    // Umbral conservador: >=14/20 muestras en HIGH = cable conectado
    return (cuenta_high >= 14) ? 1 : 0;
}


/* ------------------------------------------------------------------
 *  Fiber_Enviar_Umbral_Temp
 *  Envia a NUCLEO-B el umbral de temperatura configurado en la web.
 *  Formato de trama: "U:<valor>\n"  (ej: "U:28\n")
 *  Se llama al arranque (app_main) y cada vez que el usuario
 *  modifica el umbral desde config.cgi.
 *
 *  NOTA: el printf de debug es temporal, eliminar cuando se estabilice.
 * ------------------------------------------------------------------ */
void Fiber_Enviar_Umbral_Temp(uint16_t temp_c) {
    char trama[16];
    snprintf(trama, sizeof(trama), "U:%u\n", (unsigned)temp_c);

    printf("[A] TX umbral: %s", trama);   // DEBUG: trama ya incluye \n

    Fiber_Send(trama);
}