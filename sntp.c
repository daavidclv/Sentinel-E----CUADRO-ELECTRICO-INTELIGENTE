#include "sntp.h"                     // Keil.MDK-Pro::Network:CORE
#include "rtc.h"
#include "cmsis_os2.h"

extern osThreadId_t TID_LDR_5;

//const NET_ADDR4 ntp_server = { NET_ADDR_IP4, 0, 217, 79, 179, 106 };
void time_callback (uint32_t seconds, uint32_t seconds_fraction);

uint32_t nowtime;
struct tm nowtm;

void time_callback (uint32_t seconds, uint32_t seconds_fraction) {
  if (seconds == 0) {
    //printf ("Server not responding or bad response.\n");
  }
  else {
    //printf ("%d seconds elapsed since 1.1.1970\n", seconds);
    nowtime = seconds + 7200; //sumamos 3600 que es una hora
    nowtm = *localtime(&nowtime);
		
		//uint8_t dia_semana = (nowtm.tm_wday == 0) ? 7 : nowtm.tm_wday;
		uint8_t dia_semana;

		if (nowtm.tm_wday == 0) {
    dia_semana = 7; // Si C dice que es 0 (Domingo), le pasamos un 7 al RTC
		} else {
    dia_semana = nowtm.tm_wday; // Para el resto de días (1 al 6), lo dejamos igual
		}
		
		
    RTC_ConfTime(nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);
    RTC_ConfDate(nowtm.tm_wday, nowtm.tm_mon, nowtm.tm_mday, nowtm.tm_year);
		
		osThreadFlagsSet(TID_LDR_5,0x08);
		
		
  }
}
