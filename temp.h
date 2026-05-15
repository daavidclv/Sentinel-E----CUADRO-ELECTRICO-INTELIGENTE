#ifndef TEMP_H
#define TEMP_H

#include "cmsis_os2.h"
#include <stdint.h>


extern osMessageQueueId_t colatemp;
extern volatile float g_temp_ultima;

void init_I2C(void);
//void init_cola(void);
float read_Temp(void);


#endif 