#include "fibra.h"
#include "cmsis_os2.h"
#include "Driver_USART.h"
#include <string.h>

extern ARM_DRIVER_USART Driver_USART7;

/* Cola de mensajes pendientes para enviar */
static osMessageQueueId_t colaTx;

/* Cola de bytes recibidos */
static osMessageQueueId_t colaRx;

/* ID del hilo TX para sincronizar con la ISR */
static osThreadId_t tid_tx;

/* Buffer donde el driver deposita cada byte recibido */
static uint8_t rx_byte;

/* Callback unico: avisa al hilo cuando termina de enviar
 *                 y encola el byte cuando recibe */
static void Fiber_Callback(uint32_t event) {
    if (event & ARM_USART_EVENT_SEND_COMPLETE) {
        osThreadFlagsSet(tid_tx, 0x01);
    }
    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        osMessageQueuePut(colaRx, &rx_byte, 0, 0);
        Driver_USART7.Receive(&rx_byte, 1);
    }
}

/* Hilo TX: espera mensajes en la cola y los envia */
/* Hilo TX: espera mensajes en la cola y los envia */
static void fiber_tx_task(void *arg) {
    char mensaje[64];
    while (1) {
        if (osMessageQueueGet(colaTx, &mensaje, NULL, osWaitForever) == osOK) {

            /* Esperar a que el USART este libre */
            while (Driver_USART7.GetStatus().tx_busy) {
                osDelay(1);
            }

            /* Limpiar cualquier flag espuria antes de enviar */
            osThreadFlagsClear(0x01);

            /* Lanzar el envio */
            Driver_USART7.Send(mensaje, strlen(mensaje));

            /* Esperar a que termine REALMENTE, con timeout de seguridad */
            uint32_t flag = osThreadFlagsWait(0x01, osFlagsWaitAny, 500U);
            if (flag >= 0x80000000U) {
                /* Timeout: el TX se atasco. Forzar abort para no quedar tirados */
                Driver_USART7.Control(ARM_USART_ABORT_SEND, 0);
            }
        }
    }
}

void Fiber_Init(void) {
    colaTx = osMessageQueueNew(10, 64, NULL);
    colaRx = osMessageQueueNew(64, sizeof(uint8_t), NULL);

    Driver_USART7.Initialize(Fiber_Callback);
    Driver_USART7.PowerControl(ARM_POWER_FULL);
    Driver_USART7.Control(ARM_USART_MODE_ASYNCHRONOUS |
                          ARM_USART_DATA_BITS_8 |
                          ARM_USART_PARITY_NONE  |
                          ARM_USART_STOP_BITS_1  |
                          ARM_USART_FLOW_CONTROL_NONE, 9600);
    Driver_USART7.Control(ARM_USART_CONTROL_TX, 1);
    Driver_USART7.Control(ARM_USART_CONTROL_RX, 1);

    /* Empezar a escuchar */
    Driver_USART7.Receive(&rx_byte, 1);

    tid_tx = osThreadNew(fiber_tx_task, NULL, NULL);
}

void Fiber_Send(char *texto) {
    osMessageQueuePut(colaTx, texto, 0, 0);
}

uint8_t Fiber_Read(void) {
    uint8_t b;
    if (osMessageQueueGet(colaRx, &b, NULL, 0) == osOK) {
        return b;
    }
    return 0;
}


//void Fiber_ReInit(void) {
//    /* Reconfigurar el USART2 tras Stop Mode.
//       Las colas, hilos y callbacks NO se tocan: viven en RAM. */
//    Driver_USART7.Control(ARM_USART_MODE_ASYNCHRONOUS |
//                          ARM_USART_DATA_BITS_8 |
//                          ARM_USART_PARITY_NONE  |
//                          ARM_USART_STOP_BITS_1  |
//                          ARM_USART_FLOW_CONTROL_NONE, 115200);
//    Driver_USART7.Control(ARM_USART_CONTROL_TX, 1);
//    Driver_USART7.Control(ARM_USART_CONTROL_RX, 1);
//    
//    /* Rearmar la escucha (tras Stop Mode la Receive() pendiente se cancela) */
//    extern uint8_t rx_byte;   /* o pasar por parametro */
//    Driver_USART7.Receive(&rx_byte, 1);
//}