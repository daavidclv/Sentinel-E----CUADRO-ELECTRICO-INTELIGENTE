#ifndef FIBRA_OPT_H
#define FIBRA_OPT_H

#include <stdint.h>

/* Inicializa USART2 a 115200, TX y RX */
void Fiber_Init(void);

/* Envia un texto por la fibra */
void Fiber_Send(char *texto);

/* Lee un byte recibido. Devuelve 0 si no hay nada. */
uint8_t Fiber_Read(void);

int Fibra_Cable_Conectado(void);
/* Envia el umbral de temperatura a NUCLEO-B.
 * Formato: "U:<temp>\n" donde <temp> es un entero en °C.
 * Llamar tras cambiar g_umbral_temp_c. */
void Fiber_Enviar_Umbral_Temp(uint16_t temp_c);
#endif