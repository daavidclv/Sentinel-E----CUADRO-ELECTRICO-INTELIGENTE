
#ifndef __rtc_H
#define __rtc_H

#include <stdint.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"


#define RTC_ASYNCH_PREDIV  0x7F   /* LSE as RTC clock */
#define RTC_SYNCH_PREDIV   0x00FF /* LSE as RTC clock */
#define RTC_BKP_DR1                       0x00000001U

void RTC_Init(void);
void RTC_WEB(char *showtime, char *showdate);

void RTC_ConfTime(uint8_t hour, uint8_t minutes, uint8_t seconds);
void RTC_ConfDate(uint8_t WeekDay, uint8_t Month, uint8_t Day, uint8_t Year);
 
#endif 
