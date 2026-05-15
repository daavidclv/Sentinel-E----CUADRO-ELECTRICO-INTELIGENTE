#include "mfrc522.h"
#include <string.h>

/* =========================================================
   Comandos PCD
   ========================================================= */
#define PCD_IDLE                0x00
#define PCD_TRANSCEIVE          0x0C
#define PCD_SOFTRESET           0x0F

/* =========================================================
   Comandos PICC
   ========================================================= */
#define PICC_CMD_REQA           0x26
#define PICC_CMD_SEL_CL1        0x93
#define PICC_CMD_CT             0x88

/* =========================================================
   Registros MFRC522
   ========================================================= */
#define CommandReg              0x01
#define ComIrqReg               0x04
#define ErrorReg                0x06
#define FIFODataReg             0x09
#define FIFOLevelReg            0x0A
#define ControlReg              0x0C
#define BitFramingReg           0x0D
#define CollReg                 0x0E
#define ModeReg                 0x11
#define TxControlReg            0x14
#define TxASKReg                0x15
#define TModeReg                0x2A
#define TPrescalerReg           0x2B
#define TReloadRegH             0x2C
#define TReloadRegL             0x2D
#define VersionReg              0x37

#define MFRC522_SPI_TIMEOUT_MS  100U
#define MFRC522_CMD_TIMEOUT_MS   200U

static void MFRC522_Seleccionar(MFRC522_HandleTypeDef *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static void MFRC522_Deseleccionar(MFRC522_HandleTypeDef *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

static void MFRC522_EscribirRegistro(MFRC522_HandleTypeDef *dev, uint8_t reg, uint8_t valor)
{
    uint8_t tx[2];

    tx[0] = (uint8_t)((reg << 1U) & 0x7EU);
    tx[1] = valor;

    MFRC522_Seleccionar(dev);
    HAL_SPI_Transmit(dev->hspi, tx, 2U, MFRC522_SPI_TIMEOUT_MS);
    MFRC522_Deseleccionar(dev);
}

static uint8_t MFRC522_LeerRegistro(MFRC522_HandleTypeDef *dev, uint8_t reg)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = (uint8_t)(((reg << 1U) & 0x7EU) | 0x80U);
    tx[1] = 0x00U;
    rx[0] = 0x00U;
    rx[1] = 0x00U;

    MFRC522_Seleccionar(dev);
    HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 2U, MFRC522_SPI_TIMEOUT_MS);
    MFRC522_Deseleccionar(dev);

    return rx[1];
}

static void MFRC522_EscribirFIFO(MFRC522_HandleTypeDef *dev, const uint8_t *datos, uint8_t longitud)
{
    uint8_t i;

    for (i = 0; i < longitud; i++)
    {
        MFRC522_EscribirRegistro(dev, FIFODataReg, datos[i]);
    }
}

static void MFRC522_LeerFIFO(MFRC522_HandleTypeDef *dev, uint8_t *datos, uint8_t longitud)
{
    uint8_t i;

    for (i = 0; i < longitud; i++)
    {
        datos[i] = MFRC522_LeerRegistro(dev, FIFODataReg);
    }
}

static void MFRC522_PonerBits(MFRC522_HandleTypeDef *dev, uint8_t reg, uint8_t mascara)
{
    uint8_t valor = MFRC522_LeerRegistro(dev, reg);
    MFRC522_EscribirRegistro(dev, reg, (uint8_t)(valor | mascara));
}

static void MFRC522_QuitarBits(MFRC522_HandleTypeDef *dev, uint8_t reg, uint8_t mascara)
{
    uint8_t valor = MFRC522_LeerRegistro(dev, reg);
    MFRC522_EscribirRegistro(dev, reg, (uint8_t)(valor & (uint8_t)(~mascara)));
}

/* =========================================================
   Inicializacion
   ========================================================= */
void MFRC522_HardReset(MFRC522_HandleTypeDef *dev)
{
    HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(dev->rst_port, dev->rst_pin, GPIO_PIN_SET);
    HAL_Delay(50);
}

void MFRC522_SoftReset(MFRC522_HandleTypeDef *dev)
{
    MFRC522_EscribirRegistro(dev, CommandReg, PCD_SOFTRESET);
    HAL_Delay(50);
}

void MFRC522_AntennaOn(MFRC522_HandleTypeDef *dev)
{
    uint8_t valor = MFRC522_LeerRegistro(dev, TxControlReg);

    if ((valor & 0x03U) != 0x03U)
    {
        MFRC522_PonerBits(dev, TxControlReg, 0x03U);
    }
}

uint8_t MFRC522_GetVersion(MFRC522_HandleTypeDef *dev)
{
    return MFRC522_LeerRegistro(dev, VersionReg);
}

HAL_StatusTypeDef MFRC522_Init(MFRC522_HandleTypeDef *dev)
{
    if ((dev == NULL) || (dev->hspi == NULL))
    {
        return HAL_ERROR;
    }

    MFRC522_Deseleccionar(dev);
    MFRC522_HardReset(dev);
    MFRC522_SoftReset(dev);

    /* Temporizador interno recomendado para transacciones basicas */
    MFRC522_EscribirRegistro(dev, TModeReg, 0x80U);
    MFRC522_EscribirRegistro(dev, TPrescalerReg, 0xA9U);
    MFRC522_EscribirRegistro(dev, TReloadRegL, 0xE8U);
    MFRC522_EscribirRegistro(dev, TReloadRegH, 0x03U);

    MFRC522_EscribirRegistro(dev, TxASKReg, 0x40U);
    MFRC522_EscribirRegistro(dev, ModeReg, 0x3DU);

    MFRC522_AntennaOn(dev);

    return HAL_OK;
}


static MFRC522_StatusTypeDef MFRC522_Transceive(MFRC522_HandleTypeDef *dev,
                                                const uint8_t *txData,
                                                uint8_t txLen,
                                                uint8_t *rxData,
                                                uint8_t *rxLen,
                                                uint8_t txLastBits,
                                                uint8_t *rxValidBits)
{
    uint8_t irqReg;
    uint8_t errorReg;
    uint8_t fifoLevel;
    uint8_t validBits;
    uint32_t t0 = HAL_GetTick();

    MFRC522_EscribirRegistro(dev, CommandReg, PCD_IDLE);
    MFRC522_EscribirRegistro(dev, ComIrqReg, 0x7FU);
    MFRC522_EscribirRegistro(dev, FIFOLevelReg, 0x80U);
    MFRC522_QuitarBits(dev, CollReg, 0x80U);

    MFRC522_EscribirFIFO(dev, txData, txLen);
    MFRC522_EscribirRegistro(dev, BitFramingReg, (uint8_t)(txLastBits & 0x07U));

    MFRC522_EscribirRegistro(dev, CommandReg, PCD_TRANSCEIVE);
    MFRC522_PonerBits(dev, BitFramingReg, 0x80U);

    do
    {
        irqReg = MFRC522_LeerRegistro(dev, ComIrqReg);

        if ((irqReg & 0x01U) != 0U)
        {
            MFRC522_QuitarBits(dev, BitFramingReg, 0x80U);
            return MFRC522_TIMEOUT;
        }

        if ((irqReg & 0x30U) != 0U)
        {
            break;
        }

    } while ((HAL_GetTick() - t0) < MFRC522_CMD_TIMEOUT_MS);

    MFRC522_QuitarBits(dev, BitFramingReg, 0x80U);

    if ((HAL_GetTick() - t0) >= MFRC522_CMD_TIMEOUT_MS)
    {
        return MFRC522_TIMEOUT;
    }

    errorReg = MFRC522_LeerRegistro(dev, ErrorReg);
    if ((errorReg & 0x13U) != 0U)
    {
        return MFRC522_ERROR;
    }

    fifoLevel = MFRC522_LeerRegistro(dev, FIFOLevelReg);
    validBits = (uint8_t)(MFRC522_LeerRegistro(dev, ControlReg) & 0x07U);

    if ((rxData != NULL) && (rxLen != NULL))
    {
        if (fifoLevel > *rxLen)
        {
            return MFRC522_ERROR;
        }

        MFRC522_LeerFIFO(dev, rxData, fifoLevel);
        *rxLen = fifoLevel;
    }

    if (rxValidBits != NULL)
    {
        *rxValidBits = validBits;
    }

    return MFRC522_OK;
}

MFRC522_StatusTypeDef MFRC522_RequestA(MFRC522_HandleTypeDef *dev, uint8_t *atqa, uint8_t *atqa_len)
{
    uint8_t cmd = PICC_CMD_REQA;
    uint8_t validBits = 0U;
    MFRC522_StatusTypeDef estado;

    if ((atqa == NULL) || (atqa_len == NULL) || (*atqa_len < 2U))
    {
        return MFRC522_ERROR;
    }

    estado = MFRC522_Transceive(dev, &cmd, 1U, atqa, atqa_len, 7U, &validBits);

    if (estado != MFRC522_OK)
    {
        return estado;
    }

    if ((*atqa_len != 2U) || (validBits != 0U))
    {
        return MFRC522_ERROR;
    }

    return MFRC522_OK;
}

/* =========================================================
   Devuelve 5 bytes: UID0 UID1 UID2 UID3 BCC
   ========================================================= */
MFRC522_StatusTypeDef MFRC522_Anticoll_CL1(MFRC522_HandleTypeDef *dev, uint8_t *uid_cl1_5bytes)
{
    uint8_t trama[2];
    uint8_t rxLen = 5U;
    uint8_t validBits = 0U;
    uint8_t bcc;
    MFRC522_StatusTypeDef estado;

    if (uid_cl1_5bytes == NULL)
    {
        return MFRC522_ERROR;
    }

    trama[0] = PICC_CMD_SEL_CL1;
    trama[1] = 0x20U;

    estado = MFRC522_Transceive(dev, trama, 2U, uid_cl1_5bytes, &rxLen, 0U, &validBits);
    if (estado != MFRC522_OK)
    {
        return estado;
    }

    if ((rxLen != 5U) || (validBits != 0U))
    {
        return MFRC522_ERROR;
    }

    bcc = (uint8_t)(uid_cl1_5bytes[0] ^
                    uid_cl1_5bytes[1] ^
                    uid_cl1_5bytes[2] ^
                    uid_cl1_5bytes[3]);

    if (bcc != uid_cl1_5bytes[4])
    {
        return MFRC522_ERROR;
    }

    return MFRC522_OK;
}

/* =========================================================
   Lee UID simple de 4 bytes
   ========================================================= */
MFRC522_StatusTypeDef MFRC522_ReadUid4(MFRC522_HandleTypeDef *dev, uint8_t uid[4])
{
    uint8_t uid_cl1[5];
    MFRC522_StatusTypeDef estado;

    estado = MFRC522_Anticoll_CL1(dev, uid_cl1);
    if (estado != MFRC522_OK)
    {
        return estado;
    }

    if (uid_cl1[0] == PICC_CMD_CT)
    {
        return MFRC522_ERROR;
    }

    memcpy(uid, uid_cl1, 4U);
    return MFRC522_OK;
}