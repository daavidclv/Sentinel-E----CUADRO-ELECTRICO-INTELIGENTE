/*------------------------------------------------------------------------------
 * HTTP_Server_CGI.c - Modulo CGI del servidor HTTP de NUCLEO-A
 *
 * Funciones que implementa (las exige el stack rl_net de Keil):
 *   netCGI_ProcessQuery -> URLs con ?clave=valor (config de red)
 *   netCGI_ProcessData  -> formularios POST (LEDs, LCD, config, etc.)
 *   netCGI_Script       -> contenido dinamico de los .cgi (SSI)
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmsis_os2.h"
#include "rl_net.h"
#include "lcd.h"
#include "adc.h"
#include "LEDs.h"
#include "historial.h"              // Persistencia de eventos en W25Q128
#include "configuracion.h"          // Variables persistentes (umbrales, IDs)
#include "aplicacion_rfid.h"        // (RFID local, ya en desuso)
#include "acceso_maestro.h"         // Logica de desbloqueo web por RFID
#include "fibra_opt.h"              // Envio/recepcion por fibra optica
#include <stdbool.h>

// Silencia warning de Clang sobre formats no literales en sprintf(buf, &env[..])
#if      defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#pragma  clang diagnostic push
#pragma  clang diagnostic ignored "-Wformat-nonliteral"
#endif

/* Timeout antes de declarar fibra caida.
   NUCLEO-B envia cada ~60s (5s activo + 55s stop) -> margen 2.5x = 150s */
#define FIBRA_TIMEOUT_MS  150000U

/* === EXTERNS de main.c y otros modulos === */
extern uint16_t AD_in (uint32_t ch);          // Lectura ADC para web
extern uint8_t  get_button (void);            // Lectura boton para web

extern uint8_t modo_leds;                     // Modo LEDs (0/1/2)
extern char lcd_text[2][20+1];                // Lineas del LCD
extern char aShowTime[80];                    // Hora formateada por RTC
extern char aShowDate[80];                    // Fecha formateada por RTC

// Estado del modulo de acceso maestro (desbloqueo via UID RFID)
extern volatile bool flag_acceso_maestro;

// Telemetria recibida de NUCLEO-B por fibra
extern volatile uint32_t g_temp_count;        // Nº tramas T:
extern volatile float    g_temp_remota;       // Ultima temperatura
extern volatile uint32_t g_ultimo_rx_fibra_tick;  // Tick de ultima trama
extern volatile uint32_t g_ultimo_error_tick; // Tick del ultimo error
extern volatile uint32_t g_fibra_errores;     // Contador de errores
extern volatile uint16_t g_consumo_mA;        // Corriente activa
extern volatile uint16_t g_bateria_mV;        // Tension pack pilas
extern volatile uint32_t g_pm_count;          // Nº tramas P:
extern volatile uint16_t g_stop_mA;           // Corriente pre-Stop
extern volatile uint32_t g_ciclos_totales;    // Ciclos completos de B


// OJO: este #define repite el anterior pero con otro valor (75000). El que vale
// es el ultimo (define manda el ultimo). El primero (150000) queda redefinido.
#define FIBRA_TIMEOUT_MS         75000U   /* trama esperada cada 60s */
#define FIBRA_ERROR_VENTANA_MS    5000U   /* errores recientes = fibra mal */


/* === Variables locales del modulo CGI === */
static uint8_t P2;                            // Estado bitmask de LEDs (web)
static uint8_t ip_addr[NET_ADDR_IP6_LEN];     // Buffer binario para IPs
static char    ip_string[40];                 // Buffer string para IPs


/* Estructura usada para mantener estado entre llamadas repetidas a netCGI_Script.
   Cuando el bit 31 de 'len' esta puesto, el stack vuelve a llamar al CGI con
   el mismo *pcgi para continuar generando contenido (paginacion). */
typedef struct {
    uint8_t idx;        // Indice de iteracion
    uint8_t unused[3];
} MY_BUF;
#define MYBUF(p)        ((MY_BUF *)p)   // Cast helper para ver *pcgi como MY_BUF


/* ------------------------------------------------------------------
 *  netCGI_ProcessQuery
 *  Procesa los parametros de la query-string de la URL.
 *  Lo usa el stack para que la web configure direcciones IPv4/IPv6,
 *  mascaras, gateway y DNS via formulario GET.
 *  Codificacion de claves:
 *     i4/i6 -> IP, m4 -> mask, g  -> gateway,
 *     p4/p6 -> DNS primario,    s4/s6 -> DNS secundario.
 * ------------------------------------------------------------------ */
void netCGI_ProcessQuery (const char *qstr) {
    netIF_Option opt = netIF_OptionMAC_Address;
    int16_t      typ = 0;
    char var[40];

    do {
        // Saca la siguiente variable "clave=valor" de la query
        qstr = netCGI_GetEnvVar (qstr, var, sizeof (var));

        // var[0] indica QUE campo de red se modifica
        switch (var[0]) {
            case 'i':
                if (var[1] == '4') opt = netIF_OptionIP4_Address;
                else               opt = netIF_OptionIP6_StaticAddress;
                break;
            case 'm':
                if (var[1] == '4') opt = netIF_OptionIP4_SubnetMask;
                break;
            case 'g':
                opt = netIF_OptionIP6_DefaultGateway;
                break;
            case 'p':
                if (var[1] == '4') opt = netIF_OptionIP4_PrimaryDNS;
                else               opt = netIF_OptionIP6_PrimaryDNS;
                break;
            case 's':
                if (var[1] == '4') opt = netIF_OptionIP4_SecondaryDNS;
                else               opt = netIF_OptionIP6_SecondaryDNS;
                break;
            default: var[0] = '\0'; break;
        }

        // var[1] indica IPv4 ('4') o IPv6 ('6')
        switch (var[1]) {
            case '4': typ = NET_ADDR_IP4; break;
            case '6': typ = NET_ADDR_IP6; break;
            default:  var[0] = '\0'; break;
        }

        // Si la variable es valida y tiene formato "Xn=valor", aplica el ajuste
        if ((var[0] != '\0') && (var[2] == '=')) {
            netIP_aton (&var[3], typ, ip_addr);                 // Texto -> binario
            netIF_SetOption (NET_IF_CLASS_ETH, opt, ip_addr,    // Aplica a interfaz
                             sizeof(ip_addr));
        }
    } while (qstr);
}


/* ------------------------------------------------------------------
 *  parsear_uid_hex
 *  Convierte texto tipo "A1B2C3D4" o "A1 B2 C3 D4" en 4 bytes.
 *  Mas tolerante que el del main.c: ignora cualquier separador no hex.
 *  Se usa al guardar el UID master desde el formulario web.
 * ------------------------------------------------------------------ */
static void parsear_uid_hex(const char* texto, uint8_t* uid_out) {
    uint32_t valor = 0;
    int idx = 0;
    int nibble = 0;

    for (int i = 0; texto[i] != '\0' && idx < 4; i++) {
        char c = texto[i];
        int v = -1;
        if      (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;

        // Si es un caracter hex valido, acumula nibble
        if (v >= 0) {
            valor = (valor << 4) | (uint32_t)v;
            nibble++;
            // Cada 2 nibbles -> 1 byte completo del UID
            if (nibble == 2) {
                uid_out[idx++] = (uint8_t)valor;
                valor = 0;
                nibble = 0;
            }
        }
    }
}


/* ------------------------------------------------------------------
 *  copiar_campo
 *  Copia un campo del POST a un destino sustituyendo '+' por ' '.
 *  Los formularios HTML codifican espacios como '+', no como %20.
 *  No hace decodificacion completa de URL-encoding (no maneja %XX).
 * ------------------------------------------------------------------ */
static void copiar_campo(const char* src, char* dst, int max_len) {
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dst[i] = (src[i] == '+') ? ' ' : src[i];
    }
    dst[i] = '\0';   // Termina string
}


/* ------------------------------------------------------------------
 *  netCGI_ProcessData
 *  Punto de entrada para datos POST. El stack llama aqui con todos
 *  los campos del formulario concatenados como "var1=valor1&var2=...".
 *
 *  code = 0 -> datos de formulario (los que procesamos)
 *  code != 0 -> otros tipos (ignorados aqui)
 *
 *  Maneja:
 *    - leds.cgi: LEDs, modo, LCD, dev_id, ubicacion
 *    - users.cgi: cambio de password (pw0/pw2)
 *    - config.cgi: ref, t_max, i_max, rfid_m (con flag_acceso_maestro)
 *    - logs.cgi: clear=YES (borrar historial)
 * ------------------------------------------------------------------ */
void netCGI_ProcessData (uint8_t code, const char *data, uint32_t len) {
    char var[80], passw[12];
    int  config_modificada = 0;   // Flag: si se modifico algo persistente

    if (code != 0) return;        // Solo procesamos code=0

    P2 = 0;                       // Resetea bitmask de LEDs
    if (len == 0) {
        LED_Expo (P2);            // Sin datos -> apaga todos los LEDs
        return;
    }
    passw[0] = 1;                 // Marcador: aun no leimos el primer pw

    do {
        // Saca siguiente variable "clave=valor"
        data = netCGI_GetEnvVar (data, var, sizeof (var));
        if (var[0] == 0) continue;

        /* ---- Estado de LEDs individuales (web manual) ---- */
        if      (strcmp(var, "led0=on") == 0) P2 |= 0x01;
        else if (strcmp(var, "led1=on") == 0) P2 |= 0x02;
        else if (strcmp(var, "led2=on") == 0) P2 |= 0x04;
        else if (strcmp(var, "led3=on") == 0) P2 |= 0x08;
        else if (strcmp(var, "led4=on") == 0) P2 |= 0x10;
        else if (strcmp(var, "led5=on") == 0) P2 |= 0x20;

        /* ---- Modo de operacion de los LEDs ---- */
        else if (strcmp(var, "ctrl=Browser")  == 0) modo_leds = 0;
        else if (strcmp(var, "ctrl=Carrusel") == 0) modo_leds = 1;
        else if (strcmp(var, "ctrl=Alarma")   == 0) modo_leds = 2;

        /* ---- Cambio de password (formulario con 2 campos identicos) ---- */
        else if ((strncmp(var, "pw0=", 4) == 0) ||
                 (strncmp(var, "pw2=", 4) == 0)) {
            if (netHTTPs_LoginActive()) {
                if (passw[0] == 1)
                    strcpy(passw, var+4);                       // 1er pw -> guarda
                else if (strcmp(passw, var+4) == 0)
                    netHTTPs_SetPassword(passw);                // 2do == 1ro -> aplica
            }
        }

        /* ---- LCD linea 1 (tambien actualiza g_id_dispositivo) ---- */
        else if (strncmp(var, "lcd1=", 5) == 0) {
            copiar_campo(var + 5, lcd_text[0], 21);
            copiar_campo(var + 5, g_id_dispositivo, CONFIG_ID_LEN);
            config_modificada = 1;
            osThreadFlagsSet(TID_Display, 0x02);                // Refresca LCD
        }
        /* ---- LCD linea 2 (tambien actualiza g_ubicacion) ---- */
        else if (strncmp(var, "lcd2=", 5) == 0) {
            copiar_campo(var + 5, lcd_text[1], 21);
            copiar_campo(var + 5, g_ubicacion, CONFIG_UBIC_LEN);
            config_modificada = 1;
            osThreadFlagsSet(TID_Display, 0x02);
        }

        /* ===== SENTINEL E: Configuracion desde config.cgi ===== */

        // ID dispositivo (alias de lcd1): sin proteccion RFID
        else if (strncmp(var, "dev_id=", 7) == 0) {
            copiar_campo(var + 7, g_id_dispositivo, CONFIG_ID_LEN);
            copiar_campo(var + 7, lcd_text[0], 21);
            config_modificada = 1;
            osThreadFlagsSet(TID_Display, 0x02);
        }
        // Ubicacion (alias de lcd2): sin proteccion RFID
        else if (strncmp(var, "loc=", 4) == 0) {
            copiar_campo(var + 4, g_ubicacion, CONFIG_UBIC_LEN);
            copiar_campo(var + 4, lcd_text[1], 21);
            config_modificada = 1;
            osThreadFlagsSet(TID_Display, 0x02);
        }
        // Tasa de refresco (segundos) - solo si la web esta desbloqueada
        else if (strncmp(var, "ref=", 4) == 0) {
            if (flag_acceso_maestro) {
                g_tasa_refresco_seg = (uint16_t)atoi(var + 4);
                config_modificada = 1;
            }
        }
        // Umbral de temperatura - solo desbloqueado
        // Al cambiarlo, lo envia tambien a NUCLEO-B por fibra
        else if (strncmp(var, "t_max=", 6) == 0) {
            if (flag_acceso_maestro) {
                g_umbral_temp_c = (uint16_t)atoi(var + 6);
                config_modificada = 1;
                Fiber_Enviar_Umbral_Temp(g_umbral_temp_c);
            }
        }
        // Umbral de corriente - solo desbloqueado
        // Lo guarda en decimas (multiplica x10 con redondeo) para evitar floats
        else if (strncmp(var, "i_max=", 6) == 0) {
            if (flag_acceso_maestro) {
                float f = (float)atof(var + 6);
                g_umbral_corriente_dA = (uint16_t)(f * 10.0f + 0.5f);
                config_modificada = 1;
            }
        }
        // Cambio de UID master - solo desbloqueado y solo si esta en lista blanca
        else if (strncmp(var, "rfid_m=", 7) == 0) {
            if (flag_acceso_maestro) {
                uint8_t nuevo_uid[4];
                parsear_uid_hex(var + 7, nuevo_uid);

                if (Config_UID_Es_Autorizado(nuevo_uid)) {
                    memcpy(g_uid_master, nuevo_uid, 4);
                    config_modificada = 1;
                    Guardar_En_Historial("INFO", aShowTime,
                                         "UID master cambiado");
                } else {
                    // UID fuera de la lista blanca -> registra intento
                    Guardar_En_Historial("ALARMA", aShowTime,
                                         "Intento de UID master no autorizado");
                }
            }
        }

        /* ===== SENTINEL E: Borrar todo el historial ===== */
        else if (strcmp(var, "clear=YES") == 0) {
            Historial_Borrar_Todo();
            Guardar_En_Historial("INFO", aShowTime,
                                 "Historial borrado por usuario");
        }

    } while (data);   // Sigue mientras netCGI_GetEnvVar devuelva otra variable

    /* Si algo de config cambio, guarda en Flash externa y deja traza */
    if (config_modificada) {
        if (Config_Guardar() == 0) {
            Guardar_En_Historial("INFO", aShowTime,
                                 "Configuracion actualizada");
        } else {
            Guardar_En_Historial("ALARMA", aShowTime,
                                 "Error al guardar configuracion");
        }
    }

    LED_Expo (P2);    // Aplica el bitmask final a los LEDs fisicos
}


/* Declaracion del lector del pin de deteccion fisica de fibra (PD6).
   Se implementa en fibra_opt.c. */
extern int Fibra_Cable_Conectado(void);

// Wrapper local: 1 si la fibra esta conectada fisicamente, 0 si no
static int fibra_esta_ok(void) {
    return Fibra_Cable_Conectado();
}


/* ------------------------------------------------------------------
 *  netCGI_Script
 *  Genera el contenido dinamico que pide el .cgi mediante etiquetas
 *  "c X..." (Server Side Includes del stack rl_net).
 *
 *  Convenio:
 *    - env contiene la etiqueta tras "c "
 *    - buf es donde escribimos el HTML (limite ~120 bytes por llamada)
 *    - Para emitir mas, devolvemos 'len' con bit 31 puesto y nos llaman
 *      de nuevo con *pcgi preservado (estado de iteracion)
 * ------------------------------------------------------------------ */
uint32_t netCGI_Script (const char *env, char *buf, uint32_t buflen, uint32_t *pcgi) {
    int32_t socket;
    netTCP_State state;
    NET_ADDR r_client;
    const char *lang;
    uint32_t len = 0U;
    uint8_t id;
    static uint32_t adv;
    netIF_Option opt = netIF_OptionMAC_Address;
    int16_t      typ = 0;

    switch (env[0]) {

        /* ===================== case 'a': direcciones IP =====================
           Formato: "a X N <fmt>" donde
              X = l/i/m/g/p/s (tipo de campo)
              N = 4 (IPv4) o 6 (IPv6)
           Lee la opcion de la interfaz, la formatea y la mete en buf. */
        case 'a' :
            // env[3] = '4' o '6'
            switch (env[3]) {
                case '4': typ = NET_ADDR_IP4; break;
                case '6': typ = NET_ADDR_IP6; break;
                default: return (0);
            }
            // env[2] = tipo de direccion
            switch (env[2]) {
                case 'l':
                    if (env[3] == '4') return (0);                 // No hay link-local IPv4
                    else opt = netIF_OptionIP6_LinkLocalAddress;
                    break;
                case 'i':
                    if (env[3] == '4') opt = netIF_OptionIP4_Address;
                    else               opt = netIF_OptionIP6_StaticAddress;
                    break;
                case 'm':
                    if (env[3] == '4') opt = netIF_OptionIP4_SubnetMask;
                    else return (0);
                    break;
                case 'g':
                    if (env[3] == '4') opt = netIF_OptionIP4_DefaultGateway;
                    else               opt = netIF_OptionIP6_DefaultGateway;
                    break;
                case 'p':
                    if (env[3] == '4') opt = netIF_OptionIP4_PrimaryDNS;
                    else               opt = netIF_OptionIP6_PrimaryDNS;
                    break;
                case 's':
                    if (env[3] == '4') opt = netIF_OptionIP4_SecondaryDNS;
                    else               opt = netIF_OptionIP6_SecondaryDNS;
                    break;
            }
            netIF_GetOption (NET_IF_CLASS_ETH, opt, ip_addr, sizeof(ip_addr));
            netIP_ntoa (typ, ip_addr, ip_string, sizeof(ip_string));    // Binario -> texto
            len = (uint32_t)sprintf (buf, &env[5], ip_string);          // Aplica format de env
            break;

        /* ===================== case 'b': LEDs y modo =====================
           b c <fmt> -> 3 "selected" segun modo_leds (Browser/Carrusel/Alarma)
           b N <fmt> -> "checked" si el bit N de P2 esta activo */
        case 'b':
            if (env[2] == 'c') {
                len = (uint32_t)sprintf (buf, &env[4],
                                         (modo_leds == 0) ? "selected" : "",
                                         (modo_leds == 1) ? "selected" : "",
                                         (modo_leds == 2) ? "selected" : "");
                break;
            }
            id = env[2] - '0';                                   // N (0-9)
            if (id > 7) id = 0;
            id = (uint8_t)(1U << id);                            // bitmask
            len = (uint32_t)sprintf (buf, &env[4], (P2 & id) ? "checked" : "");
            break;

        /* ===================== case 'c': tabla TCP sockets =====================
           Itera sockets TCP del stack y emite una fila por socket.
           Usa MYBUF(pcgi)->idx como contador. Bit 31 -> seguir iterando. */
        case 'c':
            while ((uint32_t)(len + 150) < buflen) {             // Mientras quepa otra fila
                socket = ++MYBUF(pcgi)->idx;
                state  = netTCP_GetState (socket);
                if (state == netTCP_StateINVALID) return ((uint32_t)len);   // Fin
                len += (uint32_t)sprintf (buf+len, "<tr align=\"center\">");
                if (state <= netTCP_StateCLOSED) {
                    len += (uint32_t)sprintf (buf+len,
                        "<td>%d</td><td>%d</td><td>-</td><td>-</td><td>-</td><td>-</td></tr>\r\n",
                        socket, netTCP_StateCLOSED);
                } else if (state == netTCP_StateLISTEN) {
                    len += (uint32_t)sprintf (buf+len,
                        "<td>%d</td><td>%d</td><td>%d</td><td>-</td><td>-</td><td>-</td></tr>\r\n",
                        socket, netTCP_StateLISTEN, netTCP_GetLocalPort(socket));
                } else {
                    netTCP_GetPeer (socket, &r_client, sizeof(r_client));
                    netIP_ntoa (r_client.addr_type, r_client.addr,
                                ip_string, sizeof (ip_string));
                    len += (uint32_t)sprintf (buf+len,
                        "<td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td><td>%d</td></tr>\r\n",
                        socket, netTCP_StateLISTEN, netTCP_GetLocalPort(socket),
                        netTCP_GetTimer(socket), ip_string, r_client.port);
                }
            }
            len |= (1u << 31);                                   // Pide nueva llamada
            break;

        /* ===================== case 'd': estado de autenticacion =====================
           d1 -> login activo/inactivo
           d2 -> password actual (en claro: solo demo) */
        case 'd':
            switch (env[2]) {
                case '1':
                    len = (uint32_t)sprintf (buf, &env[4],
                        netHTTPs_LoginActive() ? "Enabled" : "Disabled");
                    break;
                case '2':
                    len = (uint32_t)sprintf (buf, &env[4], netHTTPs_GetPassword());
                    break;
            }
            break;

        /* ===================== case 'e': idioma activo ===================== */
        case 'e':
            lang = netHTTPs_GetLanguage();
            if      (strncmp(lang, "en", 2) == 0) lang = "English";
            else if (strncmp(lang, "de", 2) == 0) lang = "German";
            else if (strncmp(lang, "fr", 2) == 0) lang = "French";
            else if (strncmp(lang, "sl", 2) == 0) lang = "Slovene";
            else lang = "Unknown";
            len = (uint32_t)sprintf (buf, &env[2], lang, netHTTPs_GetLanguage());
            break;

        /* ===================== case 'f': texto del LCD ===================== */
        case 'f':
            switch (env[2]) {
                case '1': len = (uint32_t)sprintf (buf, &env[4], lcd_text[0]); break;
                case '2': len = (uint32_t)sprintf (buf, &env[4], lcd_text[1]); break;
            }
            break;

        /* ===================== case 'g': estados [OK]/[REVISAR] =====================
           g1 -> NUCLEO-A (siempre OK si la web responde)
           g2 -> NUCLEO-B (OK si la fibra esta fisicamente conectada)
           g3 -> Comunicacion bidireccional (mismo criterio que g2)
           g4 -> Alarmas activas (estatico de momento) */
        case 'g':
            switch (env[2]) {
                case '1':
                    len = (uint32_t)sprintf(buf,
                        "<span class=\"ok\">[ OK ]</span>");
                    break;
                case '2':
                    if (fibra_esta_ok()) {
                        len = (uint32_t)sprintf(buf,
                            "<span class=\"ok\">[ OK ]</span>");
                    } else {
                        len = (uint32_t)sprintf(buf,
                            "<span class=\"al\">[ REVISAR ]</span>");
                    }
                    break;
                case '3':
                    if (fibra_esta_ok()) {
                        len = (uint32_t)sprintf(buf,
                            "<span class=\"ok\">[ OK ]</span>");
                    } else {
                        len = (uint32_t)sprintf(buf,
                            "<span class=\"al\">[ REVISAR ]</span>");
                    }
                    break;
                case '4':
                    // TODO: enganchar con logica real de alarmas
                    len = (uint32_t)sprintf(buf,
                        "<span class=\"al\">[ REVISAR ]</span>");
                    break;
            }
            break;

        /* ===================== case 'h': fecha y hora del RTC =====================
           Solo lee aShowDate/aShowTime; ya NO escribe en lcd_text como antes
           (que era un side effect peligroso al renderizar el HTML). */
        case 'h':
            switch (env[2]) {
                case '1':
                    len = (uint32_t)sprintf (buf, &env[4], aShowDate);
                    break;
                case '2':
                    len = (uint32_t)sprintf (buf, &env[4], aShowTime);
                    break;
            }
            break;

        /* ===================== case 'z': historial de eventos =====================
           Itera todos los eventos del historial guardado en flash externa.
           Orden inverso: mas reciente primero.
           Cada llamada emite una fila <tr>. Bit 31 puesto -> sigue iterando. */
        case 'z':
        {
            char buffer_evento[TAMANO_MENSAJE];
            uint32_t total_eventos = Historial_Num_Eventos();

            // Caso especial: no hay eventos
            if (total_eventos == 0) {
                if (MYBUF(pcgi)->idx == 0) {
                    len += (uint32_t)sprintf(buf + len,
                        "<tr><td colspan=\"3\" style=\"text-align:center;\">"
                        "No hay eventos registrados en la memoria.</td></tr>\r\n");
                }
                return ((uint32_t)len);
            }

            uint32_t paso = MYBUF(pcgi)->idx;
            if (paso >= total_eventos) {
                return ((uint32_t)len);       // Hemos emitido ya todo
            }

            // Convierte indice de iteracion a indice fisico (orden inverso)
            uint32_t indice_flash = total_eventos - 1U - paso;
            Leer_De_Historial(indice_flash, buffer_evento);

            // 0xFF = celda borrada de la flash -> aborta
            if ((uint8_t)buffer_evento[0] == 0xFF) {
                return ((uint32_t)len);
            }

            // Formato guardado: "TIPO | TIMESTAMP | DESCRIPCION"
            char *tipo = strtok(buffer_evento, "|");
            char *hora = strtok(NULL, "|");
            char *desc = strtok(NULL, "|");

            if (tipo && hora && desc) {
                // ALARMA en rojo, el resto en azul claro
                if (strstr(tipo, "ALARMA")) {
                    len += (uint32_t)sprintf(buf + len,
                        "<tr><td>%s</td>"
                        "<td style=\"color:#FF3333;font-weight:bold;\">%s</td>"
                        "<td>%s</td></tr>\r\n",
                        hora, tipo, desc);
                } else {
                    len += (uint32_t)sprintf(buf + len,
                        "<tr><td>%s</td>"
                        "<td style=\"color:#43D7FF;\">%s</td>"
                        "<td>%s</td></tr>\r\n",
                        hora, tipo, desc);
                }
            }

            MYBUF(pcgi)->idx++;                // Siguiente evento en proxima llamada
            len |= (1u << 31);                 // Pide otra vuelta
        }
        break;

        /* ===================== case 'k': inputs del config.cgi =====================
           Generamos el HTML completo desde aqui porque los .cgi de Keil
           tienen un limite de longitud muy ajustado (FCARM).
           k1 -> input ID dispositivo
           k2 -> input Ubicacion
           k3 -> input Tasa de refresco
           k4 -> input Umbral temperatura
           k5 -> input Umbral corriente
           k6 -> <select> de UIDs autorizados (paginado)
           k7 -> indicador BLOQUEADO/DESBLOQUEADO */
        case 'k':
        {
            // Si la web esta bloqueada -> inputs disabled
            const char* attr = flag_acceso_maestro ? "" : "disabled";

            switch (env[2]) {
                case '1':
                    // ID dispositivo siempre editable (alias de LCD line 1)
                    len = (uint32_t)sprintf(buf,
                        "<input name=\"dev_id\" class=\"in\" value=\"%s\">",
                        g_id_dispositivo);
                    break;
                case '2':
                    // Ubicacion siempre editable (alias de LCD line 2)
                    len = (uint32_t)sprintf(buf,
                        "<input name=\"loc\" class=\"in\" value=\"%s\">",
                        g_ubicacion);
                    break;
                case '3':
                    len = (uint32_t)sprintf(buf,
                        "<input type=\"number\" name=\"ref\" class=\"in\" value=\"%u\" %s>",
                        (unsigned)g_tasa_refresco_seg, attr);
                    break;
                case '4':
                    len = (uint32_t)sprintf(buf,
                        "<input type=\"number\" name=\"t_max\" class=\"in\" value=\"%u\" %s>",
                        (unsigned)g_umbral_temp_c, attr);
                    break;
                case '5': {
                    // El umbral se guarda en decimas; lo mostramos como float
                    float val = g_umbral_corriente_dA / 10.0f;
                    len = (uint32_t)sprintf(buf,
                        "<input type=\"number\" name=\"i_max\" class=\"in\" value=\"%.1f\" %s>",
                        val, attr);
                    break;
                }
                case '6': {
                    /* Genera <select> con todas las tarjetas autorizadas.
                       Necesitamos varias llamadas porque buf es chico.
                       Usamos *pcgi como contador de estado:
                         paso==0           -> emite "<select ...>"
                         paso==1..N        -> emite la opcion (paso-1)
                         paso==N+1         -> emite "</select>" y termina */
                    uint32_t paso = *pcgi;

                    if (paso == 0U) {
                        // Apertura del select
                        len = (uint32_t)sprintf(buf,
                            "<select name=\"rfid_m\" class=\"in\" %s>", attr);
                        *pcgi = 1U;
                        len |= (1U << 31);
                    }
                    else if (paso <= (uint32_t)NUM_UIDS_AUTORIZADOS) {
                        // Una <option> por iteracion
                        uint32_t i = paso - 1U;
                        const uint8_t* u = UIDS_AUTORIZADOS[i];
                        // Marca como "selected" el que coincida con el master actual
                        const char* sel = (memcmp(u, g_uid_master, 4) == 0)
                                          ? "selected" : "";

                        len = (uint32_t)sprintf(buf,
                            "<option value=\"%02X%02X%02X%02X\" %s>%02X:%02X:%02X:%02X</option>",
                            u[0], u[1], u[2], u[3], sel,
                            u[0], u[1], u[2], u[3]);

                        *pcgi = paso + 1U;
                        len |= (1U << 31);
                    }
                    else {
                        // Cierre del select y fin
                        len = (uint32_t)sprintf(buf, "</select>");
                        // Sin bit 31: el stack avanza a la siguiente linea del .cgi
                    }
                    break;
                }
                case '7':
                    // Indicador visual del estado de desbloqueo
                    if (flag_acceso_maestro)
                        len = (uint32_t)sprintf(buf,
                            "<b style=\"color:#0F0\">DESBLOQUEADO</b>");
                    else
                        len = (uint32_t)sprintf(buf,
                            "<b style=\"color:#F00\">BLOQUEADO: Pase Tarjeta</b>");
                    break;
            }
        }
        break;

        /* ===================== case 't': telemetria de NUCLEO-B =====================
           Datos que llegan por fibra. Los .cgi system y energia usan estos.
           t1 -> Temperatura (de trama T:)
           t2 -> Corriente activa (de trama P:)
           t3 -> Pie informativo
           t4 -> Tension bateria (de trama P:)
           t5 -> UID master configurado
           t6 -> Cuenta atras hasta proximo ciclo
           t7 -> % bateria con color
           t8 -> Modo (ACTIVO / STOP MODE / SIN SENYAL)
           t9 -> Corriente pre-Stop (de trama S:)
           ta -> Contador de ciclos completos
           tb -> Consumo medio estimado del ciclo */
        case 't':
            switch (env[2]) {
                case '1':
                    // Si no se ha recibido aun ninguna temp, muestra "--"
                    if (g_temp_count == 0) {
                        len = (uint32_t)sprintf(buf,
                            "--<span style=\"font-size:14px;\">&deg;C</span>");
                    } else {
                        len = (uint32_t)sprintf(buf,
                            "%.1f<span style=\"font-size:14px;\">&deg;C</span>",
                            g_temp_remota);
                    }
                    break;

                case '2':
                    // Corriente activa: usa mA hasta 1000, despues A
                    if (g_pm_count == 0) {
                        len = (uint32_t)sprintf(buf,
                            "--<span style=\"font-size:14px;\">mA</span>");
                    } else if (g_consumo_mA < 1000U) {
                        len = (uint32_t)sprintf(buf,
                            "%u<span style=\"font-size:14px;\">mA</span>",
                            (unsigned)g_consumo_mA);
                    } else {
                        len = (uint32_t)sprintf(buf,
                            "%.2f<span style=\"font-size:14px;\">A</span>",
                            g_consumo_mA / 1000.0f);
                    }
                    break;

                case '3':
                    // Pie con info del numero de muestras o aviso de "sin datos"
                    if (g_temp_count == 0 && g_pm_count == 0) {
                        len = (uint32_t)sprintf(buf,
                            "<p style=\"color:#888;font-style:italic;font-size:12px;\">"
                            "Sin datos de NUCLEO-B (esperando fibra...)</p>");
                    } else {
                        uint32_t n = (g_temp_count > g_pm_count) ? g_temp_count : g_pm_count;
                        len = (uint32_t)sprintf(buf,
                            "<p style=\"color:#888;font-style:italic;font-size:12px;\">"
                            "Ultima actualizacion: muestra n=%u</p>", (unsigned)n);
                    }
                    break;

                case '4':
                    // Tension del pack (mV -> V con 2 decimales)
                    if (g_pm_count == 0) {
                        len = (uint32_t)sprintf(buf,
                            "--<span style=\"font-size:14px;\">V</span>");
                    } else {
                        len = (uint32_t)sprintf(buf,
                            "%.2f<span style=\"font-size:14px;\">V</span>",
                            g_bateria_mV / 1000.0f);
                    }
                    break;

                case '5':
                    // UID master configurado en formato AA:BB:CC:DD
                    if (g_uid_master[0] == 0 && g_uid_master[1] == 0 &&
                        g_uid_master[2] == 0 && g_uid_master[3] == 0) {
                        len = (uint32_t)sprintf(buf, "-- -- -- --");
                    } else {
                        len = (uint32_t)sprintf(buf,
                            "%02X:%02X:%02X:%02X",
                            g_uid_master[0], g_uid_master[1],
                            g_uid_master[2], g_uid_master[3]);
                    }
                    break;

                case '6':
                    // Cuenta atras hasta el proximo ciclo (60s nominal)
                    if (g_pm_count == 0) {
                        len = (uint32_t)sprintf(buf, "--:--");
                    } else {
                        uint32_t ahora    = osKernelGetTickCount();
                        uint32_t elapsed  = (ahora - g_ultimo_rx_fibra_tick) / 1000U;
                        uint32_t restante = (elapsed < 60U) ? (60U - elapsed) : 0U;
                        len = (uint32_t)sprintf(buf, "00:%02u:%02u",
                              (unsigned)(restante / 60U),
                              (unsigned)(restante % 60U));
                    }
                    break;

                case '7':
                    // Porcentaje de bateria estimado entre 3300 mV (vacia) y 4500 mV (llena)
                    if (g_pm_count == 0) {
                        len = (uint32_t)sprintf(buf, "--%");
                    } else {
                        uint32_t vmin = 3300U;
                        uint32_t vmax = 4500U;
                        uint32_t v    = (uint32_t)g_bateria_mV;
                        if (v < vmin) v = vmin;                   // Clamp inferior
                        if (v > vmax) v = vmax;                   // Clamp superior
                        uint32_t pct  = (v - vmin) * 100U / (vmax - vmin);
                        // Codigo de colores: verde / naranja / rojo
                        const char* color = (pct > 50U) ? "#00FF00" :
                                            (pct > 20U) ? "#FFA500" : "#FF3333";
                        len = (uint32_t)sprintf(buf,
                            "<span style=\"color:%s\">%u%%</span>",
                            color, (unsigned)pct);
                    }
                    break;

                case '8':
                    // Estado de NUCLEO-B segun tiempo desde ultima trama:
                    //   < 10s   -> ACTIVO  (acaba de transmitir)
                    //   < 150s  -> STOP MODE (en ciclo de bajo consumo)
                    //   >= 150s -> SIN SENYAL (probable fallo)
                    if (g_pm_count == 0) {
                        len = (uint32_t)sprintf(buf,
                            "<span style=\"color:#888\">SIN DATOS</span>");
                    } else {
                        uint32_t elapsed = (osKernelGetTickCount() - g_ultimo_rx_fibra_tick) / 1000U;
                        if (elapsed < 10U) {
                            len = (uint32_t)sprintf(buf,
                                "<span style=\"color:#00FF00\">ACTIVO</span>");
                        } else if (elapsed < 150U) {
                            len = (uint32_t)sprintf(buf,
                                "<span style=\"color:#FFA500\">STOP MODE</span>");
                        } else {
                            len = (uint32_t)sprintf(buf,
                                "<span style=\"color:#FF3333\">SIN SENYAL</span>");
                        }
                    }
                    break;

                case '9':
                    // Corriente medida antes de entrar a Stop (aprox al consumo Stop real)
                    if (g_ciclos_totales == 0) {
                        len = (uint32_t)sprintf(buf,
                            "--<span style=\"font-size:14px;\">mA</span>");
                    } else {
                        len = (uint32_t)sprintf(buf,
                            "%u<span style=\"font-size:14px;\">mA</span>",
                            (unsigned)g_stop_mA);
                    }
                    break;

                case 'a':
                    // Contador total de ciclos completados por B
                    len = (uint32_t)sprintf(buf,
                        "%lu", (unsigned long)g_ciclos_totales);
                    break;

                case 'b':
                    // Consumo medio del ciclo: I_media = (I_act*5 + I_stop*55)/60
                    if (g_ciclos_totales == 0) {
                        len = (uint32_t)sprintf(buf,
                            "--<span style=\"font-size:14px;\">mA</span>");
                    } else {
                        uint32_t i_media = ((uint32_t)g_consumo_mA * 5U +
                                            (uint32_t)g_stop_mA  * 55U) / 60U;
                        len = (uint32_t)sprintf(buf,
                            "%lu<span style=\"font-size:14px;\">mA</span>",
                            (unsigned long)i_media);
                    }
                    break;
            }
            break;

        /* ===================== case 's': endpoint AJAX de estado =====================
           Devuelve JSON {"u":<unlocked>,"s":<segundos_restantes>}.
           Lo consume el JavaScript del config.cgi para refrescar el estado
           de desbloqueo sin recargar la pagina entera. */
        case 's':
            {
                uint32_t segs = Acceso_Maestro_Segundos_Restantes();
                len = (uint32_t)sprintf(buf,
                    "{\"u\":%d,\"s\":%lu}",
                    flag_acceso_maestro ? 1 : 0,
                    (unsigned long)segs);
            }
            break;

        /* ===================== case 'x': lectura ADC canal 10 ===================== */
        case 'x':
            adv = AD_in (10);
            len = (uint32_t)sprintf (buf, &env[1], adv);
            break;

        /* ===================== case 'y': estado de boton para checkbox HTML ===================== */
        case 'y':
            len = (uint32_t)sprintf (buf,
                "<checkbox><id>button%c</id><on>%s</on></checkbox>",
                env[1], (get_button () & (1 << (env[1]-'0'))) ? "true" : "false");
            break;
    }
    return (len);
}