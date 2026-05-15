#include "cmsis_os2.h"                          // CMSIS RTOS header file
#include "stm32f4xx_hal.h"
#include "Driver_I2C.h"

#define DIRECCION 0x48
#define CONVERSION 0.125
#define flag 0x01

/*----------------------------------------------------------------------------
 *      Thread 1 'Thread_Name': Sample thread
 *---------------------------------------------------------------------------*/
 
static osThreadId_t tid_Thread;                        // thread id
 
extern ARM_DRIVER_I2C            Driver_I2C1;
static ARM_DRIVER_I2C *I2Cdrv = &Driver_I2C1;
static volatile uint32_t I2C_Event;

void init_I2C(void);
float read_Temp(void);
void Threadtemp (void *argument);                   // thread function
void init_cola(void);
void recoger_medida(uint32_t event);
osMessageQueueId_t colatemp;
const osThreadAttr_t thread1_attrt = {
  .stack_size = 1024                            // Creamos el hilo con menos memoria
};
float debug_temp;
static float lectura = 0.0f;
int Init_Thread_temp (void) {
 
  tid_Thread = osThreadNew(Threadtemp, NULL, &thread1_attrt);
  if (tid_Thread == NULL) {
    return(-1);
  }
 
  return(0);
}
 
void Threadtemp (void *argument) {
 init_I2C();
 init_cola();
  while (1) {
    lectura = read_Temp();
    osMessageQueuePut(colatemp, &lectura, 0, 0); 
		osDelay(1000);
		
		// Insert thread code here...
    osThreadYield();                            // suspend thread
  }
}


void init_I2C(void){/*Inicializamos el I2C(No se que hace cada cosa, lo he cpiado de un texto de ejemplo xd)*/
I2Cdrv->Initialize (NULL);/*Esto lo que hace es decir: cuando acabes llamame a esta funcion*/
I2Cdrv->PowerControl(ARM_POWER_FULL);
I2Cdrv->Control      (ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST);
I2Cdrv->Control      (ARM_I2C_BUS_CLEAR, 0);
}

void init_cola(void){
colatemp = osMessageQueueNew(10, sizeof(float), NULL); /*Incializamos la coladonde voy a guardar la variable*/
}

float read_Temp(void){/*con esto leo una temperatura*/
    float temperatura;
    uint8_t datos[2];
    I2C_Event = 0U;
    uint8_t cmd = 0x00;
  I2Cdrv->MasterTransmit (DIRECCION, &cmd, 1, true);/*Esto lo explico mas abajo*/
  
  while(I2Cdrv->GetStatus().busy);
  
  I2Cdrv->MasterReceive (DIRECCION, datos, 2, false);
  
  while(I2Cdrv->GetStatus().busy);
  
  temperatura = ((datos[0]<<8)|(datos[1]))>>5;
  temperatura = temperatura*CONVERSION;
	debug_temp = temperatura;
  return temperatura;
}



/* I2Cdrv->MasterTransmit (1, 2, 3, 4) /I2Cdrv->MasterReceive(1,2,3,4)*/
/*1-> Direccion del dispositivo esclavo en el bus I2C . Es la direccion de 7 bits o 10 bits que identifica al chip.
2-> Donde se almacenaran los datos recibidos.
3-> Numero de bytes que se quieren leer desde el esclavo.
4->  Indica si se debe enviar una condicion de STOP al finalizar la recepcion.
true ? se envia STOP (se libera el bus).
false ? se mantiene el bus ocupado (util para operaciones encadenadas, como un repeated start).*/




