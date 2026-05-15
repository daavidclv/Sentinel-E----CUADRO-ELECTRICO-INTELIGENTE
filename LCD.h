#ifndef LCD_H
#define LCD_H


#include "stm32f4xx_hal.h"
#include "Driver_SPI.h"
#include "cmsis_os2.h"                          // CMSIS RTOS header file
#include "RTE_Components.h"


int Init_Thread_LCD (void);

extern osMessageQueueId_t colaLCD_L1;
extern osMessageQueueId_t colaLCD_L2;

extern osThreadId_t TID_Display;
//Funciones inicializacion/spi
void GPIO_Init(void);
void LCD_reset(void);
void delay (uint32_t n_microsegundos);
void LCD_init(void);
void LCD_wr_data(unsigned char letra);
void LCD_wr_cmd(unsigned char cmd);
void Iniciacion_LCD(void);
void recibe(void);
void reset_buffer(void);
//funcion rutina array datos a LCD
void LCD_update(void);

//funcion arial
void symbolToLocalBuffer_L1(uint8_t symbol);
void symbolToLocalBuffer_L2(uint8_t symbol);

//Funcion para escribir
void escribe(char* linea1, char* linea2);
void escribir_txtl1(char*texto);
void escribir_txtl2(char*texto);
void LCD_init(void);
/*
Inicializar el TIM7
Inicializar el SPI(RTE_Device MOSI PA7;SCK PA5)
Primero funcion Iniciacion_LCD()
Segundo 
*/


#endif