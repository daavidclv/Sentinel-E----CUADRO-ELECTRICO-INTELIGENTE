#ifndef __SNTP_H
#define __SNTP_H

#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE
#include "stm32f4xx_hal.h"              // Keil::Device:STM32Cube HAL:Common
#include "cmsis_os2.h"
#include "time.h"

extern uint32_t nowtime;
extern struct tm nowtm;

	// Declarations
void get_time (void);
void time_callback (uint32_t seconds, uint32_t seconds_fraction);

#endif  /* __SNTP_H */
