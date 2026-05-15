#ifndef __STM32F4xx_LP_MODES_H
#define __STM32F4xx_LP_MODES_H

#include "main.h"
#include "rtc.h"
#include "stm32f4xx_hal.h"
#include "rl_net.h"
#include "LEDs.h"


void SleepMode(void);
void BotonAzul_Init_WAKEUP (void);
void ETH_PhyEnterPowerDownMode(void);
void ETH_PhyExitFromPowerDownMode(void);
void Enter_Stop_Mode(void);            
void Configurar_WakeUp_RTC_55s(void);   
#endif 