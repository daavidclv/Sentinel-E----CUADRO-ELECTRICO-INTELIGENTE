#ifndef FIBRA_H
#define FIBRA_H

#include <stdint.h>

/* Inicializa USART2 a 115200, TX y RX */
void Fiber_Init(void);

/* Envia un texto por la fibra */
void Fiber_Send(char *texto);

/* Lee un byte recibido. Devuelve 0 si no hay nada. */
uint8_t Fiber_Read(void);


void Fiber_ReInit(void);
#endif