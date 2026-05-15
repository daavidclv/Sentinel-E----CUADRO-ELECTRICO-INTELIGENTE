/*-----------------------------------------------------------------------------
 * app_main.c - NUCLEO-B (nodo remoto autonomo Sentinel E)
 *
 * ESTADO ACTUAL: FASE PM (sobre FASE D/G)
 *   - LPWR_Mode coordina TEMP y RFID mediante flags
 *   - Boton azul (PC13) -> ciclo TEMP + RFID
 *   - Timer RTC 55s     -> ciclo solo TEMP
 *   - Stop Mode real con wake-up por RTC/EXTI
 *   - FASE PM: TEMP hace 5 lecturas en 5s y EN PARALELO mide corriente.
 *              Al final del ciclo envia T:<media_temp> y P:<media_mA>,<vbat_mV>
 *   - FASE PM: LPWR mide corriente pre-Stop y envia S:<mA>
 *
 * FLAGS DE COORDINACION (bits del osThreadFlags):
 *   0x10  -> LPWR despierta por timer RTC (solo TEMP)
 *   0x20  -> LPWR despierta por boton EXTI (TEMP + RFID)
 *   0x40  -> LPWR ordena a TEMP y RFID que empiecen (mismo bit, hilos distintos)
 *   0x80  -> TEMP avisa a LPWR de que termino
 *   0x100 -> RFID avisa a LPWR de que termino (bit separado para evitar colision)
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "main.h"
#include "rtc.h"
#include "LCD.h"
#include "LEDs.h"
#include "adc.h"
#include "stm32f4xx_hal.h"
#include "botonazul_rst.h"
#include "temp.h"                   // Driver LM75A (I2C)
#include "aplicacion_rfid.h"        // Driver MFRC522 (SPI5)
#include "cmsis_os2.h"
#include "lowpower_mode.h"          // Enter_Stop_Mode, Configurar_WakeUp_RTC_55s
#include "fibra.h"                  // Fiber_Send, Fiber_Read, Fiber_Init
#include "alarma_motor.h"           // Logica de alarma por umbral de temperatura
//#include "Thread_adc.h"           // FASE PM: medida integrada en TEMP, ya no se usa


/* --- Stack del thread principal (multiplo de 8 bytes por AAPCS) --- */
#define APP_MAIN_STK_SZ (1024U)
uint64_t app_main_stk[APP_MAIN_STK_SZ / 8];
const osThreadAttr_t app_main_attr = {
  .stack_mem  = &app_main_stk[0],
  .stack_size = sizeof(app_main_stk)
};

/* Stack dedicado para el antiguo hilo TempFibra (ya no se usa, se conserva) */
static uint64_t tempfibra_stk[1024 / 8];
static const osThreadAttr_t tempfibra_attr = {
    .name = "TempFibra", .stack_mem = tempfibra_stk, .stack_size = sizeof(tempfibra_stk)
};

/* --- Variables globales --- */
ADC_HandleTypeDef adchandle;                            // Handle ADC1 de NUCLEO-B
char lcd_text[2][20+1] = { "NUCLEO-B FaseC", "Esperando..." };

volatile float    g_ultima_temperatura  = 0.0f;   // Ultima temp leida por TEMP
volatile uint32_t g_lpwr_ciclos         = 0;      // Contador de ciclos run+stop
volatile uint32_t g_lpwr_estado         = 0;      // Estado actual (debug)

/* FASE PM: ultimas medidas validas del subsistema analogico */
volatile uint16_t g_ultima_corriente_mA = 0;      // Corriente activa (trama P:)
volatile uint16_t g_ultimo_voltaje_mV   = 0;      // Tension bateria (trama P:)

/* Duracion del Stop Mode entre ciclos (55s para que el ciclo total sea ~60s) */
#define LPWR_PERIODO_TIMER_MS    55000U

/* --- Flags de coordinacion entre hilos --- */
#define FLAG_DESPIERTA_TIMER     0x10U    // Wake-up por RTC -> solo TEMP
#define FLAG_DESPIERTA_BOTON     0x20U    // Wake-up por EXTI -> TEMP + RFID
#define FLAG_TEMP_DESPIERTA      0x40U    // LPWR ordena inicio a TEMP
#define FLAG_TEMP_TERMINADO      0x80U    // TEMP avisa fin a LPWR
#define FLAG_RFID_DESPIERTA      0x40U    // LPWR ordena inicio a RFID (mismo bit, hilo distinto)
#define FLAG_RFID_TERMINADO      0x100U   // RFID avisa fin a LPWR (bit separado para WaitAll)

/* --- IDs de todos los hilos --- */
osThreadId_t TID_Display;     // LCD
osThreadId_t TID_Rtc;         // RTC
osThreadId_t TID_VIVO;        // Heartbeat LED
osThreadId_t TID_TEMP;        // Adquisicion temperatura + corriente
osThreadId_t TID_LPWR;        // Gestor del ciclo run/stop
osThreadId_t TID_TEST;        // (no usado)
osThreadId_t TID_FIBER_TX;    // (no usado, TX se hace desde TEMP/LPWR directamente)
osThreadId_t TID_FIBER_RX;    // (no usado aqui, ver TID_FibraRX)
osThreadId_t TID_TempFibra;   // (obsoleto, conservado)
osThreadId_t TID_FibraRX;     // Parser de comandos recibidos de NUCLEO-A

/* Colas de mensajes */
extern osMessageQueueId_t colatemp;     // Temperatura hacia consumidores internos
extern osMessageQueueId_t colaLCD_L1;  // Texto LCD linea 1
extern osMessageQueueId_t colaLCD_L2;  // Texto LCD linea 2
extern osThreadId_t id_tarea_rfid;     // ID del hilo RFID (creado por Aplicacion_RFID)

/* --- Prototipos internos --- */
static void Display        (void *arg);
static void CLKRTC         (void *arg);
static void VIVO           (void *arg);
static void TEMP           (void *arg);
static void LPWR_Mode      (void *arg);
static void Fiber_Rx_Thread(void *arg);

__NO_RETURN void app_main (void *arg);


/*============================================================================
  Display: gestor del LCD de NUCLEO-B.
  Espera flag 0x02 y repinta las 2 lineas de lcd_text.
  Identico al de NUCLEO-A; el LCD se mantiene en B durante todo el proyecto.
============================================================================*/
static __NO_RETURN void Display(void *arg) {
  (void)arg;
  LCD_reset();
  Iniciacion_LCD();
  reset_buffer();
  while(1) {
    osThreadFlagsWait(0x02, osFlagsWaitAny, osWaitForever);
    reset_buffer();
    escribe(lcd_text[0], lcd_text[1]);
    LCD_update();
  }
}

/*============================================================================
  CLKRTC: inicializa el RTC de NUCLEO-B.
  Solo init; el RTC se usa para wake-up en Stop Mode, no para timestamps.
============================================================================*/
static void CLKRTC (void *arg) {
  (void)arg;
  RTC_Init();
  while(1) { osDelay(1000); }
}

/*============================================================================
  VIVO: heartbeat del LED1 (azul) a 5 Hz.
  Indica que NUCLEO-B esta en Run Mode y el RTOS corre con normalidad.
============================================================================*/
static void VIVO (void *arg) {
  (void)arg;
  while(1) {
    LED_Encendido(1); osDelay(100);
    LED_Apagado(1);   osDelay(100);
  }
}

/*============================================================================
  TEMP - FASE PM:
  Ciclo de adquisicion de 5 lecturas en 5 segundos.
  En cada lectura:
    - Lee temperatura del LM75A (I2C)
    - Lee corriente del INA180A3 (ADC canal 13, PC3)
    - Lee tension de bateria del divisor R=47k/56k (ADC canal 10, PC0)
  Al final calcula media de corriente y envia por fibra:
    T:<ultima_temp>\n
    P:<media_mA>,<vbat_mV>\n
  Luego avisa a LPWR con FLAG_TEMP_TERMINADO.

  Conversion corriente:
    V_shunt = V_ADC (salida INA180A3, ganancia 20 V/V sobre shunt 0.1 Ohm)
    I_mA = V_ADC[V] * 1000 / (ganancia * R_shunt) = V_ADC * 1000 / (20 * 0.02)
    Nota: 20*0.02 = 0.4, asi que I_mA = V_ADC * 2500. Revisar si el shunt es
    realmente 0.02 Ohm o 0.1 Ohm (en propuesta dice 0.1 Ohm, ganancia INA180A3=100)

  Conversion voltaje bateria (divisor R1=47k, R2=56k):
    Vbat = Vadc * (R1+R2)/R2 = Vadc * 103/56 = Vadc * 1.8393
    -> factor 1839 para obtener mV directamente
============================================================================*/
static void TEMP (void *arg) {
  (void)arg;
  float    lectura = 0.0f;
  static char buf_fibra[40];
  int i;

  ADC_HandleTypeDef hadc_pm;     // Handle ADC local (se reinicia tras Stop Mode)
  uint32_t suma_mA = 0;
  uint16_t v_mV    = 0;

  while(1) {
    /* 1. Esperar orden de LPWR para empezar el ciclo */
    osThreadFlagsWait(FLAG_TEMP_DESPIERTA, osFlagsWaitAny, osWaitForever);

    /* 2. Re-inicializar perifericos: Stop Mode desactiva clocks de I2C y ADC */
    init_I2C();
    ADC1_pins_F429ZI_config();
    ADC_Init_Single_Conversion(&hadc_pm, ADC1);

    /* 3. Reset acumuladores del ciclo */
    suma_mA = 0;
    v_mV    = 0;

    /* 4. 5 lecturas separadas 1 segundo */
    for (i = 0; i < 5; i++) {
      /* --- Temperatura LM75A por I2C --- */
      lectura = read_Temp();
      g_ultima_temperatura = lectura;
      printf("TEMP: %.1f C\r\n", lectura);

      /* Notifica al motor de alarma (compara con umbral recibido de A) */
      Motor_Alarma_OnNewTemperature(lectura);

      /* Publica la temperatura en la cola interna */
      osMessageQueuePut(colatemp, &lectura, 0, 0);

      /* --- Medida analogica del subsistema PCB --- */
      {
        float    v_adc;
        uint16_t i_mA;

        /* Canal 13 (PC3): salida del INA180A3 -> corriente de consumo */
        v_adc = ADC_getVoltage(&hadc_pm, 13);
        i_mA  = (uint16_t)(v_adc * 1000.0f / (20.0f * 0.02f));
        suma_mA += i_mA;

        /* Canal 10 (PC0): salida del divisor R1=47k/R2=56k -> tension bateria */
        v_adc = ADC_getVoltage(&hadc_pm, 10);
        v_mV  = (uint16_t)(v_adc * 1839.0f);

        printf("[B] ADC: I=%u mA, V=%u mV\r\n", (unsigned)i_mA, (unsigned)v_mV);
      }

      /* Actualiza LCD con la temperatura actual */
      snprintf(lcd_text[1], 21, "Temp: %.1f C", lectura);
      osThreadFlagsSet(TID_Display, 0x02);
      osDelay(1000);
    }

    /* 5. Calcula media y envia tramas por fibra */
    {
      uint16_t media_mA = (uint16_t)(suma_mA / 5U);
      g_ultima_corriente_mA = media_mA;
      g_ultimo_voltaje_mV   = v_mV;

      /* Trama T: temperatura (ultima de las 5 lecturas) */
      snprintf(buf_fibra, sizeof(buf_fibra), "T:%.1f\n", lectura);
      Fiber_Send(buf_fibra);
      printf("[B] TX fibra: %s", buf_fibra);

      /* Trama P: corriente media + tension bateria */
      snprintf(buf_fibra, sizeof(buf_fibra), "P:%u,%u\n",
               (unsigned)media_mA, (unsigned)v_mV);
      Fiber_Send(buf_fibra);
      printf("[B] TX fibra: %s", buf_fibra);
      printf("[B] Media corriente activo: %u mA (5 muestras)\r\n",
             (unsigned)media_mA);
    }

    /* 6. Avisar a LPWR de que hemos terminado */
    osThreadFlagsSet(TID_LPWR, FLAG_TEMP_TERMINADO);
  }
}

/*============================================================================
  LPWR_Mode - FASE D/PM:
  Gestor principal del ciclo de vida de NUCLEO-B.

  Flujo de cada ciclo:
    1. RUN MODE: enciende LED0, lanza TEMP (y RFID si fue boton)
    2. Espera flags de fin (WaitAll)
    3. Si hay alarma activa, repite ciclos TEMP hasta que se desactive
    4. Medida de corriente pre-Stop -> envia trama S:<mA>
    5. Configura wake-up RTC a 55s y entra en Stop Mode
    6. Tras despertar, identifica motivo (timer o boton) y repite

  motivo_despertar arranca como BOTON para forzar un ciclo completo
  (TEMP + RFID) en el primer arranque del sistema.
============================================================================*/
static void LPWR_Mode (void *arg) {
  (void)arg;
  uint32_t motivo_despertar;
  uint32_t flags_a_esperar;

  /* Primer ciclo: tratar como si hubiera pulsado el boton (ciclo completo) */
  motivo_despertar = FLAG_DESPIERTA_BOTON;

  while(1) {

    /* === RUN MODE === */
    LED_Encendido(0);       // LED0 (verde) encendido = sistema activo
    g_lpwr_ciclos++;
    g_lpwr_estado = 2;      // 2 = RUN (para debug)

    if (motivo_despertar == FLAG_DESPIERTA_TIMER) {
      /* Wake-up por RTC: solo ciclo de temperatura */
      printf("LPWR: ciclo por TIMER (55s) -> solo TEMP\r\n");
      osThreadFlagsSet(TID_TEMP, FLAG_TEMP_DESPIERTA);
      flags_a_esperar = FLAG_TEMP_TERMINADO;
    } else {
      /* Wake-up por boton: ciclo completo TEMP + RFID en paralelo */
      printf("LPWR: ciclo por BOTON -> TEMP + RFID\r\n");
      osThreadFlagsSet(TID_TEMP,      FLAG_TEMP_DESPIERTA);
      osThreadFlagsSet(id_tarea_rfid, FLAG_RFID_DESPIERTA);
      flags_a_esperar = FLAG_TEMP_TERMINADO | FLAG_RFID_TERMINADO;
    }

    /* Espera a que todos los hilos lanzados terminen (WaitAll) */
    osThreadFlagsWait(flags_a_esperar, osFlagsWaitAll, osWaitForever);

    /* Limpia flags de wake-up que puedan haber llegado durante el ciclo */
    osThreadFlagsClear(FLAG_DESPIERTA_BOTON | FLAG_DESPIERTA_TIMER);

    printf("LPWR: ciclo completado, durmiendo %lu ms\r\n",
           (unsigned long)LPWR_PERIODO_TIMER_MS);

    /* Segunda limpieza por si acaso (doble clear intencionado) */
    osThreadFlagsClear(FLAG_DESPIERTA_BOTON | FLAG_DESPIERTA_TIMER);

    /* Si la alarma esta activa, inhibir Stop Mode hasta que se resuelva.
       Repite ciclos de TEMP cada vez (para seguir monitorizando temperatura). */
    while (Motor_Alarma_IsActive()) {
        printf("LPWR: alarma activa, inhibiendo Stop\r\n");
        osThreadFlagsSet(TID_TEMP, FLAG_TEMP_DESPIERTA);
        osThreadFlagsWait(FLAG_TEMP_TERMINADO, osFlagsWaitAny, osWaitForever);
    }

    /* === FASE PM: medida de corriente pre-Stop ===
     * TEMP y RFID han terminado. Solo corren VIVO, FibraRX y el RTOS.
     * Esta medida es la mejor aproximacion al consumo real en Stop Mode
     * que podemos obtener sin instrumentacion externa.
     * Se envia como trama S:<mA> para que A la distinga del consumo activo. */
    {
        ADC_HandleTypeDef hadc_stop;
        char buf_stop[24];

        ADC1_pins_F429ZI_config();
        if (ADC_Init_Single_Conversion(&hadc_stop, ADC1) == 0) {
            /* Canal 13 (PC3): salida INA180A3 -> corriente instantanea */
            float    v    = ADC_getVoltage(&hadc_stop, 13);
            uint16_t i_mA = (uint16_t)(v * 1000.0f / (20.0f * 0.02f));

            snprintf(buf_stop, sizeof(buf_stop), "S:%u\n", (unsigned)i_mA);
            Fiber_Send(buf_stop);
            printf("[B] TX fibra (pre-Stop): %s", buf_stop);
        } else {
            printf("[B] ERROR: ADC init fallo en pre-Stop\r\n");
        }
    }

    /* Espera 100ms a que la trama S: salga fisicamente por la fibra
       antes de apagar los clocks en Stop Mode */
    osDelay(100);

    LED_Apagado(0);         // LED0 apagado = entrando en Stop Mode

    /* Configura alarma RTC para despertar en 55 segundos */
    Configurar_WakeUp_RTC_55s();

    /* === STOP MODE === Detiene el nucleo. El sistema se queda aqui
       hasta que el RTC o el EXTI del boton generen una interrupcion. */
    Enter_Stop_Mode();

    /* === Tras despertar de Stop Mode ===
       El callback de lowpower_mode.c habrá enviado FLAG_DESPIERTA_BOTON o
       FLAG_DESPIERTA_TIMER. Esperamos con timeout corto (100ms) para
       capturarlos; si no llega ninguno, asumimos timer (caso seguro). */
    motivo_despertar = osThreadFlagsWait(FLAG_DESPIERTA_BOTON | FLAG_DESPIERTA_TIMER,
                                          osFlagsWaitAny, 100U);

    if (motivo_despertar >= 0x80000000U) {
        /* Error de osThreadFlagsWait (timeout o flags invalidas) -> asumir timer */
        printf("LPWR: WARNING wake-up sin flag identificada\r\n");
        motivo_despertar = FLAG_DESPIERTA_TIMER;
    } else if (motivo_despertar & FLAG_DESPIERTA_BOTON) {
        motivo_despertar = FLAG_DESPIERTA_BOTON;
    } else {
        motivo_despertar = FLAG_DESPIERTA_TIMER;
    }

    /* Pequeña espera para que el PLL y los perifericos se estabilicen
       tras el SystemClock_Config() que se llama al salir de Stop Mode */
    osDelay(50);

    /* Pide el umbral actual a NUCLEO-A (por si cambio mientras dormia) */
    Fiber_Send("?H\n");
    osDelay(150);           // Margen para recibir la respuesta antes del ciclo
  }
}

/*============================================================================
  Fiber_Rx_Thread - FASE G:
  Parser de comandos recibidos de NUCLEO-A por fibra optica.
  Ensambla bytes hasta '\n' y procesa la linea completa.

  Comandos soportados:
    H:<valor>\n  -> nuevo umbral de temperatura en grados Celsius
                   (lo envia A cuando cambia t_max en config.cgi)
============================================================================*/
static void Fiber_Rx_Thread(void *arg) {
    (void)arg;
    static char linea[64];
    static int pos = 0;

    while (1) {
        uint8_t b = Fiber_Read();         // Bloquea hasta recibir un byte

        if (b == 0) { osDelay(10); continue; }   // Byte nulo -> ignorar

        if (b == '\n' || b == '\r') {
            /* Fin de linea: procesar si hay contenido */
            if (pos > 0) {
                linea[pos] = '\0';
                printf("[B] RX linea: %s\r\n", linea);

                /* Comando H: actualiza umbral de alarma de temperatura */
                if (linea[0] == 'H' && linea[1] == ':') {
                    int temp = atoi(&linea[2]);
                    if (temp > 0 && temp < 150) {
                        Motor_Alarma_SetThreshold((float)temp);
                    } else {
                        printf("[B] Umbral fuera de rango: %d\r\n", temp);
                    }
                }
                pos = 0;    // Resetea buffer para siguiente linea
            }
        } else {
            /* Acumula byte en el buffer de linea */
            if (pos < (int)sizeof(linea) - 1) {
                linea[pos++] = (char)b;
            } else {
                /* Overflow: descarta la linea entera */
                printf("[B] RX overflow, descartando linea\r\n");
                pos = 0;
            }
        }
    }
}

/*============================================================================
  app_main: inicializa NUCLEO-B y lanza todos los hilos.
  Orden de init importante:
    1. Colas de mensajes
    2. Perifericos (boton, LEDs, RFID, fibra, alarma)
    3. Peticion inicial de umbral a NUCLEO-A
    4. Lanzamiento de hilos
    5. osThreadExit (libera el stack de app_main)
============================================================================*/
__NO_RETURN void app_main (void *arg) {
  (void)arg;

  /* Colas de mensajes */
  colaLCD_L1 = osMessageQueueNew(5, 20*sizeof(char), NULL);
  colaLCD_L2 = osMessageQueueNew(5, 20*sizeof(char), NULL);
  colatemp   = osMessageQueueNew(10, sizeof(float), NULL);   // Temperatura interna

  /* Perifericos */
  BotonAzul_Init();             // EXTI en PC13 con debounce 500ms
  LED_Init();                   // GPIOs de los 3 LEDs de usuario
  Aplicacion_RFID_Inicializar(); // SPI5 + MFRC522 (inicializa pero no lanza hilo aun)
  Fiber_Init();                 // USART2 + colas TX/RX de fibra

  /* Motor de alarma: compara temperatura con umbral y activa LED rojo */
  if (Motor_Alarma_Init() != 0) {
      printf("[FATAL] Motor_Alarma_Init fallo\r\n");
      while (1) {}   // Error critico: no continuar
  }

  /* Pide umbral inicial a NUCLEO-A antes de lanzar los hilos
     (3s de margen para que la fibra este lista) */
  osDelay(3000);
  Fiber_Send("?H\n");

  /* Lanzamiento de hilos */
  TID_Display = osThreadNew(Display,         NULL, NULL);
  TID_Rtc     = osThreadNew(CLKRTC,          NULL, NULL);
  TID_VIVO    = osThreadNew(VIVO,            NULL, NULL);
  TID_TEMP    = osThreadNew(TEMP,            NULL, NULL);
  TID_LPWR    = osThreadNew(LPWR_Mode,       NULL, NULL);
  TID_FibraRX = osThreadNew(Fiber_Rx_Thread, NULL, NULL);

  /* Lanza el hilo RFID (crea id_tarea_rfid que usa LPWR_Mode) */
  Aplicacion_RFID_Crear_Tarea();

  /* === FASE PM: Init_ThADC eliminado ===
   * La medida de corriente y voltaje esta ahora integrada en TEMP y LPWR_Mode.
   * No hace falta un hilo ADC independiente. */
  // if (Init_ThADC() != 0) { ... }

  osThreadExit();    // Libera el stack de app_main (1 KB)
}

/* Redireccion de printf hacia ITM/SWO (igual que en NUCLEO-A) */
struct __FILE { int handle; };
FILE __stdout;
FILE __stdin;
int fputc(int ch, FILE *f) { return (ITM_SendChar(ch)); }