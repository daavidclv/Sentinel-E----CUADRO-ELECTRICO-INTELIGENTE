#include "Thread_adc.h"
#include "fibra.h"           /* === NUEVO: para Fiber_Send === */
#include <stdio.h>           /* === NUEVO: para snprintf y printf === */

/*----------------------------------------------------------------------------
 *      Thread 'ThPot' : lee el potenciometro periodicamente
 *---------------------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
osThreadId_t       tid_ThADC;
osMessageQueueId_t id_MsgADCQueue;
/* Private prototypes --------------------------------------------------------*/
static __NO_RETURN void ThADC (void *argument);
/* Public functions ----------------------------------------------------------*/
int Init_ThADC (void) {
	/* Crea cola de mensajes del potenciometro */
	id_MsgADCQueue = osMessageQueueNew(8, sizeof(messageADC_t), NULL);

	/* Crea hilo lector del potenciometro */
	tid_ThADC = osThreadNew(ThADC, NULL, NULL);
	if (tid_ThADC == NULL) {
			return -1;
	}
	return 0;
}
/* Private functions ---------------------------------------------------------*/
static __NO_RETURN void ThADC (void *argument) {
	(void)argument;
	ADC_HandleTypeDef adchandle;       /* handler definition */
	ADC1_pins_F429ZI_config();          /* specific PINS configuration */
	ADC_Init_Single_Conversion(&adchandle , ADC1);  /* ADC1 configuration */

	messageADC_t change;
	change.voltage = 0;
	change.current = 0;
	messageADC_t msg;

	/* ===buffer para enviar la trama P: por fibra ===
	 * static: el contenido sobrevive entre iteraciones (innecesario aqui
	 * pero no estorba) y evita ocupar la pila del thread en cada vuelta. */
	static char buf_fibra[32];

	for (;;) {
		/* Lee valor */
		msg.voltage = (uint16_t)((ADC_getVoltage(&adchandle, 10))*1000);
		msg.current = (uint16_t)((ADC_getVoltage(&adchandle, 13))*1000/(20*0.02));

		if (change.voltage != msg.voltage || change.current != msg.current){
			/* Publica el valor en la cola (no bloqueante) */
			osMessageQueuePut(id_MsgADCQueue, &msg, NULL, osWaitForever);
			change.voltage = msg.voltage;
			change.current = msg.current;

			/* === NUEVO: envio por fibra a NUCLEO-A ===
			 * Formato: "P:<corriente_mA>,<voltaje_mV>\n"
			 * Igual patron que la trama T: de temperatura. */
			snprintf(buf_fibra, sizeof(buf_fibra), "P:%u,%u\n",
			         (unsigned)msg.current, (unsigned)msg.voltage);
			Fiber_Send(buf_fibra);
			printf("[B] TX fibra: %s", buf_fibra);
		}
		osDelay(250);
		osThreadYield();
	}
}
/*********************************END OF FILE**********************************/