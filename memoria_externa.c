/*------------------------------------------------------------------------------
 *  Driver Flash SPI externa para W25Q128FV / GD25Q128
 *  Proyecto Sentinel E - ISE 2025/2026
 *
 *  Funciones expuestas:
 *    - Memoria_Init        : inicializa SPI3 y el GPIO del CS
 *    - Memoria_ReadID      : lee el JEDEC ID para verificar conectividad
 *    - Memoria_ReadData    : lectura continua de datos
 *    - Memoria_WritePage   : escritura de hasta 256 bytes en una pagina
 *    - Memoria_EraseSector : borrado de un sector de 4 KiB
 *
 *  Funciones internas (static):
 *    - WriteEnable, WaitBusy, GPIO_Init
 *----------------------------------------------------------------------------*/

#include "memoria_externa.h"

/* Driver SPI3 configurado en el RTE de Keil */
extern ARM_DRIVER_SPI Driver_SPI3;
static ARM_DRIVER_SPI* const SPI_Mem_drv = &Driver_SPI3;

/* Macros de control del Chip Select (PD2) */
#define CS_LOW()   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET)

/* Espera activa hasta que el driver CMSIS SPI deje de estar ocupado */
static inline void SPI_WaitReady(void) {
    while (SPI_Mem_drv->GetStatus().busy) { /* spin */ }
}

/* ------------------------------------------------------------------
 *  Inicializacion del GPIO para el Chip Select (PD2)
 * ------------------------------------------------------------------ */
static void GPIO_Mem_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitStruct.Pin   = GPIO_PIN_2;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    CS_HIGH();   /* memoria deseleccionada al arrancar */
}

/* ------------------------------------------------------------------
 *  Memoria_Init
 *  Inicializa el driver CMSIS SPI3 en modo maestro, SPI Mode 0, 8 bits.
 *  Velocidad: 1 MHz (margen de seguridad para cables de protoboard).
 * ------------------------------------------------------------------ */
void Memoria_Init(void) {
    GPIO_Mem_Init();

    SPI_Mem_drv->Initialize(NULL);
    SPI_Mem_drv->PowerControl(ARM_POWER_FULL);
    SPI_Mem_drv->Control(ARM_SPI_MODE_MASTER |
                         ARM_SPI_CPOL0_CPHA0 |
                         ARM_SPI_MSB_LSB     |
                         ARM_SPI_DATA_BITS(8),
                         1000000U);
}

/* ------------------------------------------------------------------
 *  Memoria_ReadID
 *  Envia el comando 0x9F y recibe 3 bytes de JEDEC ID.
 * ------------------------------------------------------------------ */
uint32_t Memoria_ReadID(void) {
    uint8_t cmd = W25Q_CMD_JEDEC_ID;
    uint8_t id[3] = {0};

    CS_LOW();
    SPI_Mem_drv->Send(&cmd, 1);
    SPI_WaitReady();
    SPI_Mem_drv->Receive(id, 3);
    SPI_WaitReady();
    CS_HIGH();

    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

/* ------------------------------------------------------------------
 *  Memoria_ReadData
 *  Lectura continua (comando 0x03): comando + direccion 24 bits + datos.
 * ------------------------------------------------------------------ */
void Memoria_ReadData(uint32_t address, uint8_t *pData, uint32_t size) {
    uint8_t cmd[4];

    cmd[0] = W25Q_CMD_READ_DATA;
    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >> 8)  & 0xFF;
    cmd[3] =  address        & 0xFF;

    CS_LOW();
    SPI_Mem_drv->Send(cmd, 4);
    SPI_WaitReady();
    SPI_Mem_drv->Receive(pData, size);
    SPI_WaitReady();
    CS_HIGH();
}

/* ------------------------------------------------------------------
 *  Memoria_WaitBusy (interna)
 *  Bloquea hasta que el bit BUSY (bit 0 del status register 1) sea 0.
 *  Imprescindible tras cada program/erase, el chip necesita tiempo interno.
 * ------------------------------------------------------------------ */
static void Memoria_WaitBusy(void) {
    uint8_t cmd = W25Q_CMD_READ_STATUS_1;
    uint8_t status_reg = 0xFF;

    CS_LOW();
    SPI_Mem_drv->Send(&cmd, 1);
    SPI_WaitReady();

    do {
        SPI_Mem_drv->Receive(&status_reg, 1);
        SPI_WaitReady();
    } while ((status_reg & 0x01) == 0x01);

    CS_HIGH();
}

/* ------------------------------------------------------------------
 *  Memoria_WriteEnable (interna)
 *  Activa el latch WEL. Obligatorio antes de page program o erase.
 * ------------------------------------------------------------------ */
static void Memoria_WriteEnable(void) {
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
    CS_LOW();
    SPI_Mem_drv->Send(&cmd, 1);
    SPI_WaitReady();
    CS_HIGH();
}

/* ------------------------------------------------------------------
 *  Memoria_EraseSector
 *  Borra un sector de 4 KiB (todos los bits a 1 -> 0xFF).
 *  La direccion se alinea automaticamente a 4 KiB.
 * ------------------------------------------------------------------ */
void Memoria_EraseSector(uint32_t address) {
    uint8_t cmd[4];

    address &= ~(W25Q_SECTOR_SIZE - 1U);   /* alinear a 4 KiB */

    Memoria_WriteEnable();

    cmd[0] = W25Q_CMD_SECTOR_ERASE;
    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >> 8)  & 0xFF;
    cmd[3] =  address        & 0xFF;

    CS_LOW();
    SPI_Mem_drv->Send(cmd, 4);
    SPI_WaitReady();
    CS_HIGH();

    Memoria_WaitBusy();
}

/* ------------------------------------------------------------------
 *  Memoria_WritePage
 *  Escribe hasta 256 bytes en una pagina. El sector debe estar borrado.
 * ------------------------------------------------------------------ */
void Memoria_WritePage(uint32_t address, uint8_t *pData, uint32_t size) {
    uint8_t cmd[4];

    if (size == 0U || size > W25Q_PAGE_SIZE) return;

    Memoria_WriteEnable();

    cmd[0] = W25Q_CMD_PAGE_PROGRAM;
    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >> 8)  & 0xFF;
    cmd[3] =  address        & 0xFF;

    CS_LOW();
    SPI_Mem_drv->Send(cmd, 4);
    SPI_WaitReady();
    SPI_Mem_drv->Send(pData, size);
    SPI_WaitReady();
    CS_HIGH();

    Memoria_WaitBusy();
}