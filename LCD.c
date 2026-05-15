
#include "lcd.h"
#include "Arial12x12.h" 
#include "cmsis_os2.h"   // CMSIS RTOS header file
#include <string.h>
#include <stdio.h>

/*Funciones necesarias*/
//Funciones inicializacion/spi
void GPIO_Init(void);
void LCD_reset(void);
void delay (uint32_t n_microsegundos);
void LCD_init(void);
void LCD_wr_data(unsigned char letra);
void LCD_wr_cmd(unsigned char cmd);
void Iniciacion_LCD(void);
void recibe(void);

//funcion rutina array datos a LCD
void LCD_update(void);

//funcion arial
void symbolToLocalBuffer_L1(uint8_t symbol);
void symbolToLocalBuffer_L2(uint8_t symbol);

//Funcion para escribir
void escribe(char* linea1, char* linea2);
void escribir_txtl1(char*texto);
void escribir_txtl2(char*texto);

/*Variables necesarias*/
TIM_HandleTypeDef htim7;
static int positionL1=0;
static int positionL2=0;
static unsigned char buffer[512]= {0};
extern ARM_DRIVER_SPI Driver_SPI1;
ARM_DRIVER_SPI* SPIdrv = &Driver_SPI1;
/*----------------------------------------------------------------------------
 *      Thread 1 'Thread_Name': Sample thread
 *---------------------------------------------------------------------------*/
 
osThreadId_t tid_Thread_LCD;  // thread id
osMessageQueueId_t colaLCD_L1;
osMessageQueueId_t colaLCD_L2;
 
void Thread_LCD (void *argument);                   // thread function
 
int Init_Thread_LCD (void) {
 
  tid_Thread_LCD = osThreadNew(Thread_LCD, NULL, NULL);
  if (tid_Thread_LCD == NULL) {
    return(-1);
  }
 
  return(0);
}

/*
void Thread_LCD (void *argument) {
  
    char *txt_recibido_L1;
    char *txt_recibido_L2;
    // Inicializar el LCD
	   Iniciacion_LCD();

    while (1) {
        // Esperar hasta que haya un mensaje en la cola
				if (osMessageQueueGet(colamensaje, &txt_recibido_L1, NULL, osWaitForever) == osOK ) {
					if(osMessageQueueGet(colamensaje, &txt_recibido_L2, NULL, osWaitForever) == osOK){
							escribe(txt_recibido_L1,txt_recibido_L2);
					}
  }
        osThreadYield();  // Liberar la CPU y permitir que otros hilos se ejecuten
    }
}
*/
//DOS LINEAS INDEPENDIENTES A VER SI FUNCIONA
void Thread_LCD (void *argument) {
  
    // 1. Memorias locales (Video Memory)
    // Guardan lo que se está mostrando actualmente. Inicializadas con espacios.
    char memoria_L1[22] = "                    "; 
    char memoria_L2[22] = "                    ";
    
    // 2. Buffers temporales para recibir datos de las colas
    char buffer_rx[22]; 
    
    // Bandera para saber si hay que repintar la pantalla
    uint8_t hay_cambios = 1; 

    // Inicializar el LCD (Hardware)
    Iniciacion_LCD();

    while (1) {
        // --- REVISAR COLA LINEA 1 ---
        // Usamos timeout 0U para NO bloquear. Si no hay mensaje, seguimos.
        if (osMessageQueueGet(colaLCD_L1, &buffer_rx, NULL, 0U) == osOK ) {
            strcpy(memoria_L1, buffer_rx); // Actualizamos memoria L1
            hay_cambios = 1;
        }

        // --- REVISAR COLA LINEA 2 ---
        if (osMessageQueueGet(colaLCD_L2, &buffer_rx, NULL, 0U) == osOK){
            strcpy(memoria_L2, buffer_rx); // Actualizamos memoria L2
            hay_cambios = 1;
        }

        // --- REPINTAR SI ES NECESARIO ---
        if (hay_cambios == 1) {
            // Borramos el buffer gráfico interno (el array 'buffer' static)
            // para evitar que se mezclen letras viejas con nuevas.
            memset(buffer, 0, 512); 
            
            // Escribimos usando las memorias actuales
            escribe(memoria_L1, memoria_L2);
            
            hay_cambios = 0; // Reset bandera
        }

        osDelay(50); // Espera 50ms (20 FPS) para no saturar la CPU revisando colas vacías
    }
}

 void GPIO_Init(void){
   GPIO_InitTypeDef GPIO_InitStruct;
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
  
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  //A0 lo ponemos a nivel alto
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13, GPIO_PIN_SET);//Nivel alto
  
	GPIO_InitStruct.Pin = GPIO_PIN_14;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  //CS
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);//Nivel alto
}
 void LCD_reset(void){//Es la q ue le mete un reset al spi, inicializa la linea y la prepara para usarse

  SPIdrv->Initialize(NULL);
  SPIdrv->PowerControl (ARM_POWER_FULL);
  SPIdrv->Control(ARM_SPI_MODE_MASTER | ARM_SPI_CPOL1_CPHA1 | ARM_SPI_MSB_LSB | ARM_SPI_DATA_BITS(8), 200000000);/*Para ver con el anal?izador usar 1M..
  AQUI ESTABA EL ERROR ARM_SPI_SS_MASTER_SW lo eliminamos por que nopsotros manejamo0s el CS*/
	//Para el reset lo pongo a nivel alto, espero 100 us, lo pongo a nivel bajo, espero 2 us y lo vuelvo a poner a nivel alto y espero 1ms


  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
  delay(10);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
  delay(1000);
}
void LCD_update(void){
 int i;
 LCD_wr_cmd(0x00); // 4 bits de la parte baja de la direcci?n a 0
 LCD_wr_cmd(0x10); // 4 bits de la parte alta de la direcci?n a 0
 LCD_wr_cmd(0xB0); // P?gina 0

 for(i=0;i<128;i++){
 LCD_wr_data(buffer[i]);
 }

 LCD_wr_cmd(0x00); // 4 bits de la parte baja de la direcci?n a 0
 LCD_wr_cmd(0x10); // 4 bits de la parte alta de la direcci?n a 0
 LCD_wr_cmd(0xB1); // P?gina 1

 for(i=128;i<256;i++){
 LCD_wr_data(buffer[i]);
 }

 LCD_wr_cmd(0x00);
 LCD_wr_cmd(0x10);
 LCD_wr_cmd(0xB2); //P?gina 2
 for(i=256;i<384;i++){
 LCD_wr_data(buffer[i]);
 }

 LCD_wr_cmd(0x00);
 LCD_wr_cmd(0x10);
 LCD_wr_cmd(0xB3); // Pagina 3

 for(i=384;i<512;i++){
 LCD_wr_data(buffer[i]);
 }
}
void LCD_init(void){
  LCD_wr_cmd(0xAE);
  LCD_wr_cmd(0xA2);
  LCD_wr_cmd(0xA0);
  LCD_wr_cmd(0xC8);
  LCD_wr_cmd(0x22);
  LCD_wr_cmd(0x2F);
  LCD_wr_cmd(0x40);
  LCD_wr_cmd(0xAF);
  LCD_wr_cmd(0x81);
  LCD_wr_cmd(0x17);/*Valor de contraste*/
  LCD_wr_cmd(0xA4);
  LCD_wr_cmd(0xA6);
}
void LCD_wr_data(unsigned char letra){

  ARM_SPI_STATUS buf1;
  // Seleccionar CS = 0;
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
 // Seleccionar A0 = 1;
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13, GPIO_PIN_SET);
 // Escribir un dato (data) usando la funci?n SPIDrv->Send(?);

 // Esperar a que se libere el bus SPI;
  SPIdrv->Send(&letra, sizeof(letra));
  do{
    buf1 = SPIdrv->GetStatus();
  }while (buf1.busy);
   // Seleccionar CS = 1
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
}
void LCD_wr_cmd(unsigned char cmd)
{
  ARM_SPI_STATUS buf2;
  // Seleccionar CS = 0;
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
 // Seleccionar A0 = 0;
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13, GPIO_PIN_RESET);
 // Escribir un dato (data) usando la funci?n SPIDrv->Send(?);

 // Esperar a que se libere el bus SPI;
  SPIdrv->Send(&cmd, sizeof(cmd));
  do{
    buf2 = SPIdrv->GetStatus();
  }while (buf2.busy);
   // Seleccionar CS = 1
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
}
void symbolToLocalBuffer_L1(uint8_t symbol){
    uint8_t i, value1, value2;
    uint16_t offset = 0;

    offset = 25 * (symbol - ' ');
      if(positionL1+Arial12x12[offset]>128){
      positionL1=0;
      }
    for (i = 0; i < 12; i++){
        value1 = Arial12x12[offset + i * 2 + 1];
        value2 = Arial12x12[offset + i * 2 + 2];
        buffer[i + positionL1] = value1;
        buffer[i + 128 + positionL1] = value2;
    }
     positionL1 = positionL1 + Arial12x12[offset];
}
void symbolToLocalBuffer_L2(uint8_t symbol){
    uint8_t i, value1, value2;
    uint16_t offset = 0;

    offset = 25 * (symbol - ' ');
      if(positionL2+Arial12x12[offset]>384){
      positionL2=0;
      }
    for (i = 0; i < 12; i++){
        value1 = Arial12x12[offset + i * 2 + 1];
        value2 = Arial12x12[offset + i * 2 + 2];
        buffer[i + 256 + positionL2] = value1;
        buffer[i + 384 + positionL2] = value2;
    }
     positionL2 = positionL2 + Arial12x12[offset];
}
void escribir_txtl1(char*texto){
	int i = 0;
	
	while(texto[i] != '\0'){

		symbolToLocalBuffer_L1(texto[i]);

		i++;
	}

}
void escribir_txtl2(char*texto){
	int i = 0;
	
	while(texto[i] != '\0'){

		symbolToLocalBuffer_L2(texto[i]);

		i++;
	}

}
void delay (uint32_t n_microsegundos){

	htim7.Instance = TIM7;
	htim7.Init.Prescaler =83;
	htim7.Init.CounterMode= TIM_COUNTERMODE_UP;
	htim7.Init.Period=n_microsegundos-1;
	
	__HAL_RCC_TIM7_CLK_ENABLE();
	
	HAL_TIM_Base_Init(&htim7);
	HAL_TIM_Base_Start(&htim7);
	
	while(__HAL_TIM_GET_FLAG(&htim7, TIM_FLAG_UPDATE)==false);
	
	__HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);
	HAL_TIM_Base_Stop(&htim7);
}
void escribe(char* linea1, char* linea2){
    positionL1=0;
    positionL2=0;
    escribir_txtl1(linea1);
    escribir_txtl2(linea2);
	LCD_update();
}
void Iniciacion_LCD(void) {
    GPIO_Init();
    LCD_reset();
    LCD_init();
}
void reset_buffer(void){
	int i=0;
	for(i=0;i<512;i++){
		buffer[i]=0x00;
	}
	positionL1=0;
	positionL2=0;
	LCD_update();
}


