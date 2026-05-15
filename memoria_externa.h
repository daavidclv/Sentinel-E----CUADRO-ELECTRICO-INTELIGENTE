#ifndef __MEMORIA_EXTERNA_H
#define __MEMORIA_EXTERNA_H
 
#include "stm32f4xx_hal.h"
#include "Driver_SPI.h"
#include <stdint.h>
#include <stdbool.h>
 
/* =============================================================
 *  Driver Flash SPI externa (W25Q128FV / GD25Q128)
 * =============================================================
 *  Chip:      128 Mbit (16 MiB) Flash NOR serie
 *  Interfaz:  SPI Mode 0, MSB first, 8 bits
 *  Pinout:    SPI3 (PC10=SCK, PC11=MISO, PC12=MOSI), CS=PD2
 *  Velocidad: 1 MHz (configurable en Memoria_Init)
 * ============================================================= */
 
/* JEDEC IDs aceptados (ambos chips son funcionalmente compatibles) */
#define W25Q128_JEDEC_ID      0xEF4018U   /* Winbond W25Q128FV */
#define GD25Q128_JEDEC_ID     0xC84018U   /* GigaDevice GD25Q128 */
#define FLASH_JEDEC_ID_OK(id) (((id) == W25Q128_JEDEC_ID) || ((id) == GD25Q128_JEDEC_ID))
 
/* Comandos SPI basicos (comunes a ambos chips) */
#define W25Q_CMD_WRITE_ENABLE    0x06
#define W25Q_CMD_READ_STATUS_1   0x05
#define W25Q_CMD_READ_DATA       0x03
#define W25Q_CMD_PAGE_PROGRAM    0x02
#define W25Q_CMD_SECTOR_ERASE    0x20
#define W25Q_CMD_JEDEC_ID        0x9F
 
/* Parametros geometricos del chip */
#define W25Q_PAGE_SIZE           256U
#define W25Q_SECTOR_SIZE         4096U
#define W25Q_TOTAL_SIZE          (16U * 1024U * 1024U)
 
/* ---- API publica ---- */
 
/**
 * @brief Inicializa el perifrico SPI3 y el GPIO del CS.
 *        Llamar una vez al arranque, antes de cualquier otra funcion.
 */
void Memoria_Init(void);
 
/**
 * @brief Lee el JEDEC ID del chip. Util para verificar conectividad.
 * @return ID de 24 bits. Usar FLASH_JEDEC_ID_OK() para validar.
 */
uint32_t Memoria_ReadID(void);
 
/**
 * @brief Lee datos de la Flash a partir de una direccion.
 * @param address Direccion de lectura (0 a W25Q_TOTAL_SIZE-1).
 * @param pData   Buffer destino.
 * @param size    Numero de bytes a leer.
 */
void Memoria_ReadData(uint32_t address, uint8_t *pData, uint32_t size);
 
/**
 * @brief Escribe hasta 256 bytes en una pagina de la Flash.
 * @note  El sector DEBE estar previamente borrado (bytes a 0xFF).
 * @note  No cruzar frontera de pagina (256 bytes).
 * @param address Direccion de escritura.
 * @param pData   Buffer con los datos.
 * @param size    Numero de bytes a escribir (maximo 256).
 */
void Memoria_WritePage(uint32_t address, uint8_t *pData, uint32_t size);
 
/**
 * @brief Borra un sector de 4 KiB (lo pone todo a 0xFF).
 * @param address Direccion dentro del sector (se alinea automaticamente a 4 KiB).
 */
void Memoria_EraseSector(uint32_t address);
 
#endif /* __MEMORIA_EXTERNA_H */