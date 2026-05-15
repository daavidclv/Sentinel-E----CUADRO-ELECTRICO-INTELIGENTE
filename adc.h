/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H
#define __ADC_H

/* Exported constants --------------------------------------------------------*/
#define SIZE_MSGQUEUE_ADC			1                  // number of Message Queue Objects
#define RESOLUTION_12B 4096U
#define VREF 3.3f


/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

typedef struct {                                // object data type
  uint8_t Buf;

} MSGQUEUE_ADC_t;

/* Exported functions ------------------------------------------------------- */

void ADC1_pins_F429ZI_config(void);
int ADC_Init_Single_Conversion(ADC_HandleTypeDef *, ADC_TypeDef  *);
uint32_t ADC_getVoltage(ADC_HandleTypeDef * , uint32_t );
int Init_MsgQueue_adc(void);
int Init_Thread_adc (void);
void Thread_adc (void *argument);




#endif /* __VOL_H */


