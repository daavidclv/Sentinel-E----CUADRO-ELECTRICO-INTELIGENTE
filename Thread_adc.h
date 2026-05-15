/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __THADC_H
#define __THADC_H
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "adc.h"
/* Exported types ------------------------------------------------------------*/
/* Mensaje que publicará el hilo del potenciómetro */
typedef struct {
    uint16_t voltage; /* tensión equivalente en mV */
	  uint16_t current; /* corriente equivalente en mA */
} messageADC_t;
/* Exported constants --------------------------------------------------------*/
/* Cola y thread del módulo POT */
extern osThreadId_t tid_ThADC;
extern osMessageQueueId_t id_MsgADCQueue;
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
  /* Exported thread functions */
  /* Inicialización del módulo (ADC + hilo productor) */
  int Init_ThADC (void);
#endif /* __THADC_H */
/*********************************END OF FILE**********************************/
