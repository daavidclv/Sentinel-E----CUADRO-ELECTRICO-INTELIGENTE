/*------------------------------------------------------------------------------
 * MDK Middleware - Component ::Network
 * HTTP_Server.c - NUCLEO-A (nodo central Sentinel E)
 *----------------------------------------------------------------------------*/

/* ============================ INCLUDES ============================ */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>                     // atof, atoi
#include "main.h"
#include "rtc.h"                        // RTC: fecha/hora del sistema
#include "LCD.h"                        // Driver LCD de la placa
#include "rl_net.h"                     // Stack de red Keil MDK-Pro (HTTP, SNTP)
#include "LEDs.h"                       // Driver LEDs (LED_Expo, LED_Encendido...)
#include "adc.h"                        // ADC de NUCLEO-A
#include "stm32f4xx_hal.h"              // HAL de STM32F4
#include "sntp.h"                       // Cliente SNTP (sincronizacion horaria)
#include "botonazul_rst.h"              // Boton azul (reset/eventos)
#include "lowpower_mode.h"              // Modos de bajo consumo (no se usa en A)
#include "memoria_externa.h"            // Driver W25Q128 (flash SPI externa)
#include "historial.h"                  // Registro de eventos en flash externa
#include "aplicacion_rfid.h"            // Driver RFID local (en desuso, se quita)
#include "fibra_opt.h"                  // UART sobre fibra optica hacia NUCLEO-B
#include "acceso_maestro.h"             // Logica desbloqueo web por UID RFID
#include "configuracion.h"              // Parametros persistentes (ID, umbrales)


#define RTE_Drivers_USART2              // Habilita driver USART2 (fibra)

/* ============================ STACKS DE HILOS ============================ */

// Stack del hilo principal de aplicacion (multiplo de 8 bytes por AAPCS)
#define APP_MAIN_STK_SZ (2048U)         // 2 KB - vigilar overflow si crece
uint64_t app_main_stk[APP_MAIN_STK_SZ / 8];
const osThreadAttr_t app_main_attr = {
  .stack_mem  = &app_main_stk[0],
  .stack_size = sizeof(app_main_stk)
};

// Stack dedicado para hilo TX de fibra (printf consume mucha pila)
static uint64_t fiber_tx_stk[1024 / 8];
static const osThreadAttr_t fiber_tx_attr = {
    .name = "FiberTx", .stack_mem = fiber_tx_stk, .stack_size = sizeof(fiber_tx_stk)
};

// Stack dedicado para hilo RX de fibra (parser + printf)
static uint64_t fiber_rx_stk[1024 / 8];
static const osThreadAttr_t fiber_rx_attr = {
    .name = "FiberRx", .stack_mem = fiber_rx_stk, .stack_size = sizeof(fiber_rx_stk)
};

/* ============================ EXTERNOS ============================ */
extern uint16_t AD_in          (uint32_t ch);                 // Lectura ADC (web)
extern uint8_t  get_button     (void);                        // Lectura boton (web)
extern void     netDHCP_Notify (uint32_t if_num, uint8_t option,
                                const uint8_t *val, uint32_t len);

extern uint8_t modo_leds;                                     // Modo actual LEDs
extern char lcd_text[2][20+1];                                // Buffer texto LCD
extern char aShowTime[80];                                    // Hora formateada
extern char aShowDate[80];                                    // Fecha formateada


/* ============================ VARIABLES GLOBALES ============================ */
// Telemetria recibida desde NUCLEO-B por fibra. Volatiles porque las escribe
// el hilo Fiber_Rx y las leen el hilo web y el historial.

volatile float    g_temp_remota = 0.0f;   // Ultima temperatura del LM75A (B)
volatile uint32_t g_temp_count  = 0;      // Contador de tramas T: recibidas
volatile uint16_t g_consumo_mA  = 0;      // Corriente activa medida en B
volatile uint16_t g_bateria_mV  = 0;      // Tension del pack de pilas
volatile uint32_t g_pm_count    = 0;      // Contador de tramas P: recibidas
volatile uint16_t g_stop_mA     = 0;      // Corriente pre-Stop (aprox Stop Mode)
volatile uint32_t g_ciclos_totales = 0;   // Ciclos completos run+stop de B

/* === FASE FIBRA - Watchdog === Tick RTOS de la ultima trama valida recibida.
   La web lo usa para detectar perdida de comunicacion con NUCLEO-B */
volatile uint32_t g_ultimo_rx_fibra_tick = 0;

ADC_HandleTypeDef adchandle;              // Handle ADC1 de NUCLEO-A (no de B)

uint8_t modo_leds = 0;                    // 0=manual web, 1=carrusel, 2=alarma
char lcd_text[2][20+1] = { "LCD line 1",  // Valores por defecto antes
                           "LCD line 2" };// de cargar config de flash

/* ============================ THREAD IDs ============================ */
// IDs de todos los hilos creados con osThreadNew.
// Se usan como destinatarios de osThreadFlagsSet desde otros hilos/callbacks.
osThreadId_t TID_Display;     // LCD
osThreadId_t TID_Led;         // LEDs (carrusel/alarma)
osThreadId_t TID_Rtc;         // RTC -> strings de hora/fecha
osThreadId_t TID_LDR_5;       // Parpadeo 5Hz tras sync SNTP
osThreadId_t TID_Sntp;        // Cliente SNTP
osThreadId_t TID_Alarma;      // Parpadeo verde 5s al saltar alarma RTC
osThreadId_t TID_LPWR;        // (no se usa en A, A no duerme)
osThreadId_t TID_VIVO;        // Heartbeat LED rojo (sistema vivo)
osThreadId_t TID_TEMP;        // (no se usa en A, temperatura viene por fibra)
osThreadId_t TID_TEST1;       // Test antiguo
osThreadId_t TID_FIBER_TX;    // TX por fibra (comentado, A no envia periodico)
osThreadId_t TID_FIBER_RX;    // RX por fibra (recibe telemetria de B)


/* ============================ DECLARACIONES DE HILOS ============================ */
static void BlinkLed (void *arg);
static void Display  (void *arg);
static void CLKRTC   (void *arg);
static void Sntp     (void *arg);
static void Alarma_RTC(void *arg);
static void LPWR_Mode(void *arg);
static void VIVO     (void *arg);
static void TestFiberA(void *arg);
static void Fiber_Tx_Thread(void *arg);
static void Fiber_Rx_Thread(void *arg);

__NO_RETURN void app_main (void *arg);   // Hilo main (no retorna nunca)


/* ============================ FUNCIONES PARA LA WEB ============================ */

uint32_t voltage = 0;

// Lectura ADC expuesta a la web (canal 10 = ADC123_IN10).
// La pagina HTML pide este valor via SSI y se muestra en bruto.
uint16_t AD_in (uint32_t ch) {
  if (ch == 10) {
        voltage = ADC_getVoltage(&adchandle, ch);   // Lee canal 10 en bruto
  }
  return (voltage);
}

// Lectura de boton expuesta a la web. Devuelve 0 fijo:
// los botones reales (azul) se gestionan por EXTI, no por polling web.
uint8_t get_button (void) {
    return 0;
}

// Callback DHCP: salta cuando la red asigna IP nueva a NUCLEO-A.
// Despierta al hilo Display para que repinte el LCD con la nueva IP.
void netDHCP_Notify (uint32_t if_num, uint8_t option, const uint8_t *val, uint32_t len) {
  (void)if_num; (void)val; (void)len;

  if (option == NET_DHCP_OPTION_IP_ADDRESS) {
    osThreadFlagsSet (TID_Display, 0x01);   // Flag bit 0 -> refrescar LCD
  }
}


/* ============================================================================
   Thread 'Display': gestor del LCD
   Espera flag 0x02 (cambio de lcd_text) y repinta. Se podria refrescar mas
   cuando cambie la IP (flag 0x01 desde netDHCP_Notify), pero aqui solo se
   atiende 0x02. Si quieres que la IP repinte el LCD, anade 0x01 al wait.
============================================================================ */
static __NO_RETURN void Display(void *arg) {
    LCD_reset();                            // Reset hardware del LCD
    Iniciacion_LCD();                       // Configuracion inicial
    reset_buffer();                         // Limpia buffer interno
    (void)arg;

    while(1) {
        osThreadFlagsWait(0x02, osFlagsWaitAny, osWaitForever);  // Espera senal
        reset_buffer();                     // Borra contenido previo
        escribe(lcd_text[0], lcd_text[1]);  // Escribe las 2 lineas
        LCD_update();                       // Vuelca buffer al LCD
    }
}


/* ============================================================================
   Thread 'BlinkLed': controla los LEDs segun el modo activo.
   modo_leds lo cambia la web:
     0 = manual (web controla LEDs uno a uno)
     1 = carrusel "running lights"
     2 = alarma (todos parpadeando a 5 Hz)
============================================================================ */
static __NO_RETURN void BlinkLed (void *arg) {
    // Patron del carrusel: 16 frames con 2 LEDs encendidos en cada uno.
    const uint8_t led_val[16] = { 0x48,0x88,0x84,0x44,0x42,0x22,0x21,0x11,
                                  0x12,0x0A,0x0C,0x14,0x18,0x28,0x30,0x50 };
    uint32_t cnt = 0U;
    bool estado_alarma = false;     // Toggle para parpadeo de alarma
    (void)arg;

    while(1) {
        if (modo_leds == 1) {
            // === Modo carrusel ===
            LED_Expo(led_val[cnt]);              // Aplica patron
            if (++cnt >= sizeof(led_val)) cnt = 0U;
            osDelay(100);                        // 10 frames/s
        }
        else if (modo_leds == 2) {
            // === Modo ALARMA: 5 Hz, todos ON/OFF ===
            estado_alarma = !estado_alarma;
            if (estado_alarma) LED_Expo(0xFF);   // Todos los LEDs ON
            else               LED_Expo(0x00);   // Todos los LEDs OFF
            osDelay(100);                        // 100 ms -> 5 Hz aprox
        }
        else {
            // === Modo manual: la web controla los LEDs ===
            osDelay(100);                        // Solo cede CPU
        }
    }
}


/* ============================================================================
   Thread 'CLKRTC': mantiene actualizados los strings aShowTime / aShowDate
   que consume la web (SSI) y el historial para timestamping.
   Refresco cada 100 ms.
============================================================================ */
static void CLKRTC (void *arg) {
    RTC_Init();                     // Inicializa periferico RTC
    while(1) {
        RTC_WEB(aShowTime, aShowDate);   // Vuelca hora/fecha a strings
        osDelay(100);
    }
}


/* ============================================================================
   Thread 'Alarma_RTC': dispara cuando salta la alarma del RTC.
   Lo despierta otro modulo con osThreadFlagsSet(TID_Alarma, 0x04).
   Hace parpadear el LED 0 (verde) durante ~5 segundos.
============================================================================ */
static void Alarma_RTC (void *arg) {
    while (1) {
        osThreadFlagsWait(0x04, osFlagsWaitAny, osWaitForever);

        // 10 iteraciones x ~350 ms = ~3.5 s de parpadeo
        // (los comentarios originales decian 5 s pero no salen las cuentas)
        for(int i = 0; i < 10; i++) {
            LED_Encendido(0);    osDelay(100);
            LED_Apagado(0);      osDelay(250);
        }
    }
}


/* ============================================================================
   Thread 'Sntp': cliente SNTP.
   Espera 5 s al arranque (a que la red este lista), pide hora, vuelca al RTC
   via callback time_callback y resincroniza cada 3 minutos.
============================================================================ */
static __NO_RETURN void Sntp (void *arg) {
    (void)arg;
    osDelay(5000);                        // Espera red lista (DHCP, etc.)
    while(1) {
        netSNTPc_GetTime(NULL, time_callback);  // Peticion SNTP -> callback
        osDelay(180000);                  // Resync cada 3 minutos
    }
}


/* ============================================================================
   Thread 'LDR_5': parpadeo de confirmacion de sincronizacion SNTP.
   Cuando time_callback recibe la hora valida, hace
   osThreadFlagsSet(TID_LDR_5, 0x08) y aqui parpadea el LED 2 a 5 Hz
   durante 4 segundos (20 ciclos x 200 ms).
============================================================================ */
static void LDR_5 (void *arg) {
    (void)arg;
    while(1) {
        osThreadFlagsWait(0x08, osFlagsWaitAny, osWaitForever);
        for (int i = 0; i < 20; i++) {   // 5 Hz durante 4 s
            LED_Encendido(2);  osDelay(100);
            LED_Apagado(2);    osDelay(100);
        }
    }
}


/* ============================================================================
   Thread 'VIVO': heartbeat. NUCLEO-A no duerme, asi que parpadea el LED 0
   a 5 Hz de forma continua para indicar "sistema vivo" en laboratorio.
============================================================================ */
static void VIVO (void *arg) {
    (void)arg;
    while(1) {
        LED_Encendido(0);  osDelay(100);
        LED_Apagado(0);    osDelay(100);
    }
}


/* ============================================================================
   parsear_uid_8hex
   Convierte un string de 8 caracteres hex ("AABBCCDD") en un array de 4 bytes.
   Devuelve 1 si OK, 0 si formato invalido.
   Se usa al recibir "U:AABBCCDD" por fibra para extraer el UID de la tarjeta.
============================================================================ */
static int parsear_uid_8hex(const char *s, uint8_t uid_out[4]) {
    int i;
    if (strlen(s) < 8) return 0;        // Validacion de longitud

    for (i = 0; i < 4; i++) {
        unsigned int byte;
        char tmp[3] = { s[i*2], s[i*2 + 1], '\0' };  // Toma 2 chars hex

        if (sscanf(tmp, "%x", &byte) != 1) return 0; // Convierte a numero
        uid_out[i] = (uint8_t)byte;
    }
    return 1;
}


/* ============================================================================
   Fiber_Rx_Thread: parser de las tramas que llegan por fibra desde NUCLEO-B.
   Ensambla bytes hasta '\n' o '\r' y procesa la linea.

   Mensajes soportados:
     T:23.5\n          -> temperatura LM75A
     U:AABBCCDD\n      -> UID leido por RFID en B
     P:<mA>,<mV>\n     -> consumo activo + tension de pilas
     S:<mA>\n          -> corriente pre-Stop (estimacion Stop Mode)

   El parser es robusto: tolera concatenacion de tramas y bytes basura.
============================================================================ */
static void Fiber_Rx_Thread(void *arg) {
    (void)arg;
    static char linea[64];          // Buffer para una linea completa
    uint32_t idx = 0;               // Indice de escritura en el buffer

    while (1) {
        uint8_t b = Fiber_Read();   // Bloquea hasta que llegue un byte

        // --- Fin de linea: procesar ---
        if (b == '\n' || b == '\r') {
            if (idx > 0) {
                linea[idx] = '\0';  // Cierra string

                printf("[A] RX cruda: '%s' (len=%u)\r\n", linea, (unsigned)idx);

                // === PARSER POSICIONAL ===
                // Recorre la linea byte a byte buscando marcas T:, U:, P:, S:
                // en cualquier posicion. Tolera tramas concatenadas.
                uint32_t i = 0;
                while (i < idx) {
                    if (i + 1 >= idx) break;

                    /* ---- T: temperatura ---- */
                    if (linea[i] == 'T' && linea[i+1] == ':') {
                        char num[12]; int j = 0;
                        i += 2;
                        // Acumula digitos, punto y signo hasta proxima marca
                        while (i < idx && j < 11) {
                            char x = linea[i];
                            if (x == 'T' || x == 'U' || x == 'P' || x == 'S') break;
                            if ((x >= '0' && x <= '9') || x == '.' || x == '-') {
                                num[j++] = x;
                            }
                            i++;
                        }
                        num[j] = '\0';
                        if (j > 0) {
                            float t = (float)atof(num);
                            g_temp_remota = t;              // Publica al sistema
                            g_temp_count++;
                            g_ultimo_rx_fibra_tick = osKernelGetTickCount();
                            printf("[A] Temp: %.1f C (n=%lu)\r\n",
                                   t, (unsigned long)g_temp_count);
                        }
                    }

                    /* ---- U: UID RFID + envio de umbral a B ---- */
                    else if (linea[i] == 'U' && linea[i+1] == ':') {
                        char hex[16]; int j = 0;
                        i += 2;
                        // Aprovecha la trama U: para reenviar el umbral
                        // actual a B (asi B siempre tiene el ultimo valor).
                        printf("[A] B pide umbral, enviando %u\r\n",
                               (unsigned)g_umbral_temp_c);
                        Fiber_Enviar_Umbral_Temp(g_umbral_temp_c);

                        // Acumula 8 chars hex
                        while (i < idx && j < 8) {
                            char x = linea[i];
                            if (x == 'T' || x == 'U' || x == 'P' || x == 'S') break;
                            if ((x >= '0' && x <= '9') ||
                                (x >= 'A' && x <= 'F') ||
                                (x >= 'a' && x <= 'f')) {
                                hex[j++] = x;
                            }
                            i++;
                        }
                        hex[j] = '\0';
                        if (j == 8) {
                            uint8_t uid[4];
                            if (parsear_uid_8hex(hex, uid)) {
                                printf("[A] UID: %02X:%02X:%02X:%02X\r\n",
                                       uid[0], uid[1], uid[2], uid[3]);
                                Acceso_Maestro_Procesar_UID(uid);   // Logica web
                                g_ultimo_rx_fibra_tick = osKernelGetTickCount();
                            }
                        }
                    }

                    /* ---- P: consumo activo (mA) + tension bateria (mV) ---- */
                    else if (linea[i] == 'P' && linea[i+1] == ':') {
                        char num_i[8], num_v[8];
                        int ji = 0, jv = 0;
                        int en_voltaje = 0;
                        i += 2;

                        // Separa por la coma: antes -> mA, despues -> mV
                        while (i < idx && ji < 7 && jv < 7) {
                            char x = linea[i];
                            if (x == 'T' || x == 'U' || x == 'P' || x == 'S') break;
                            if (x == ',')          en_voltaje = 1;
                            else if (x >= '0' && x <= '9') {
                                if (!en_voltaje) num_i[ji++] = x;
                                else             num_v[jv++] = x;
                            }
                            i++;
                        }
                        num_i[ji] = '\0';
                        num_v[jv] = '\0';

                        if (ji > 0 && jv > 0) {
                            int i_mA = atoi(num_i);
                            int v_mV = atoi(num_v);
                            // Validacion de rango (descarta tramas corruptas)
                            if (i_mA >= 0 && i_mA < 2000 &&
                                v_mV >= 0 && v_mV < 10000) {
                                g_consumo_mA = (uint16_t)i_mA;
                                g_bateria_mV = (uint16_t)v_mV;
                                g_pm_count++;
                                g_ultimo_rx_fibra_tick = osKernelGetTickCount();
                                printf("[A] Consumo: %d mA, Vbat: %d mV (n=%lu)\r\n",
                                       i_mA, v_mV, (unsigned long)g_pm_count);
                            }
                        }
                    }

                    /* ---- S: corriente pre-Stop (FASE PM) ---- */
                    else if (linea[i] == 'S' && linea[i+1] == ':') {
                        char num[8]; int j = 0;
                        i += 2;
                        while (i < idx && j < 7) {
                            char x = linea[i];
                            if (x == 'T' || x == 'U' || x == 'P' || x == 'S') break;
                            if (x >= '0' && x <= '9') num[j++] = x;
                            i++;
                        }
                        num[j] = '\0';
                        if (j > 0) {
                            int s_mA = atoi(num);
                            if (s_mA >= 0 && s_mA < 500) {
                                g_stop_mA = (uint16_t)s_mA;
                                g_ciclos_totales++;     // Cuenta ciclo completo
                                g_ultimo_rx_fibra_tick = osKernelGetTickCount();
                                printf("[A] Stop Mode: %d mA (ciclo #%lu)\r\n",
                                       s_mA, (unsigned long)g_ciclos_totales);

                                // Historial cada 5 ciclos (no saturar flash)
                                if ((g_ciclos_totales % 5) == 0) {
                                    char msg_hist[64];
                                    snprintf(msg_hist, sizeof(msg_hist),
                                             "I_act=%umA I_stop=%umA V=%u.%02uV",
                                             (unsigned)g_consumo_mA,
                                             (unsigned)g_stop_mA,
                                             (unsigned)(g_bateria_mV / 1000),
                                             (unsigned)((g_bateria_mV % 1000) / 10));
                                    Guardar_En_Historial("PWR", aShowTime, msg_hist);
                                }
                            }
                        }
                    }

                    else {
                        i++;    // Byte que no inicia marca conocida -> descartar
                    }
                }
            }
            idx = 0;            // Resetea buffer para la siguiente linea
        }
        // --- Acumulando bytes de la linea actual ---
        else if (idx < 63) {
            // Solo acepta ASCII imprimible (filtra ruido binario)
            if (b >= 32 && b < 127) linea[idx++] = b;
        }
        else {
            idx = 0;            // Overflow -> descarta linea entera
        }
        // OJO: no hay osDelay porque Fiber_Read() ya bloquea hasta recibir
    }
}


/* ============================================================================
   app_main: hilo principal de la aplicacion.
   Inicializa perifericos, modulos, red y lanza todos los hilos.
   Termina con osThreadExit() (libera su stack, ya no se necesita).
============================================================================ */
__NO_RETURN void app_main (void *arg) {
    (void)arg;

    // Colas de mensajes para que la web pueda escribir en el LCD
    colaLCD_L1 = osMessageQueueNew(5, 20*sizeof(char), NULL);
    colaLCD_L2 = osMessageQueueNew(5, 20*sizeof(char), NULL);

    // --- Inicializacion de perifericos basicos ---
    BotonAzul_Init();                       // EXTI del boton azul (PC13)
    LED_Init();                             // GPIOs de los LEDs
    ADC1_pins_F429ZI_config();              // Pines del ADC1 en NUCLEO-A
    ADC_Init_Single_Conversion(&adchandle, ADC1);   // ADC1 modo single-shot

    // --- Memoria externa, historial y configuracion ---
    Memoria_Init();                         // Init driver W25Q128 (SPI)
    printf("=== Arranque del sistema ===\r\n");
    Config_Cargar();                        // Lee config persistente de flash

    // === SENTINEL E ===
    // Sincroniza el LCD con la config recien cargada de flash, asi al
    // reiniciar la placa el LCD muestra ID/Ubicacion guardados en vez
    // de los placeholders "LCD line 1" / "LCD line 2".
    strncpy(lcd_text[0], g_id_dispositivo, 20);  lcd_text[0][20] = '\0';
    strncpy(lcd_text[1], g_ubicacion,      20);  lcd_text[1][20] = '\0';

    Init_Thread_Historial();                // Lanza hilo de gestion del log

    // Modulo de acceso maestro: gestiona desbloqueo de la web cuando llega
    // por fibra el UID de la tarjeta maestra leida en NUCLEO-B
    Acceso_Maestro_Init();

    // --- RFID local DESHABILITADO ---
    // El RFID se ha movido a NUCLEO-B; se accede via fibra (mensajes U:)
    //Aplicacion_RFID_Inicializar();
    //Aplicacion_RFID_Crear_Tarea();

    // --- Enlace por fibra optica ---
    Fiber_Init();                                   // Init USART2 (fibra)
    osDelay(2000);                                  // Da tiempo a B a arrancar
    Fiber_Enviar_Umbral_Temp(g_umbral_temp_c);      // Empuja umbral inicial a B

    // --- Red (HTTP, SNTP, DHCP...) ---
    netInitialize();                                // Stack TCP/IP

    // --- Lanzamiento de hilos de aplicacion ---
    TID_Led     = osThreadNew(BlinkLed,        NULL, NULL);
    TID_Display = osThreadNew(Display,         NULL, NULL);
    TID_Rtc     = osThreadNew(CLKRTC,          NULL, NULL);
    TID_LDR_5   = osThreadNew(LDR_5,           NULL, NULL);
    TID_Sntp    = osThreadNew(Sntp,            NULL, NULL);
    TID_Alarma  = osThreadNew(Alarma_RTC,      NULL, NULL);
    TID_VIVO    = osThreadNew(VIVO,            NULL, NULL);

    // Hilo TX de fibra DESACTIVADO: A solo envia bajo demanda
    // (umbrales, comandos), no en bucle.
    //TID_FIBER_TX = osThreadNew(Fiber_Tx_Thread, NULL, &fiber_tx_attr);
    TID_FIBER_RX = osThreadNew(Fiber_Rx_Thread, NULL, &fiber_rx_attr);

    // Espera a que la hora sea coherente y registra evento de arranque
    osDelay(5000);
    Guardar_En_Historial("INFO", aShowTime, "Sistema arrancado");

    osThreadExit();             // Libera el stack de app_main (2 KB)
}


/* ============================================================================
   Redireccion de printf hacia ITM/SWO.
   Cada char escrito con printf se envia por SWO y aparece en el ITM Viewer
   de Keil. NUCLEO-A todavia depende del ST-LINK para esto.
============================================================================ */
struct __FILE { int handle; };
FILE __stdout;
FILE __stdin;

int fputc(int ch, FILE *f) {
    return (ITM_SendChar(ch));      // Envia caracter por SWO
}