#ifndef MFRC522_H
#define MFRC522_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef enum
{
    MFRC522_OK = 0,
    MFRC522_ERROR,
    MFRC522_TIMEOUT,
    MFRC522_NO_TAG
} MFRC522_StatusTypeDef;

typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
} MFRC522_HandleTypeDef;

/* Inicializacion y control basico */
HAL_StatusTypeDef MFRC522_Init(MFRC522_HandleTypeDef *dev);
void MFRC522_HardReset(MFRC522_HandleTypeDef *dev);
void MFRC522_SoftReset(MFRC522_HandleTypeDef *dev);
void MFRC522_AntennaOn(MFRC522_HandleTypeDef *dev);
uint8_t MFRC522_GetVersion(MFRC522_HandleTypeDef *dev);

/* Operaciones minimas para validacion */
MFRC522_StatusTypeDef MFRC522_RequestA(MFRC522_HandleTypeDef *dev, uint8_t *atqa, uint8_t *atqa_len);
MFRC522_StatusTypeDef MFRC522_Anticoll_CL1(MFRC522_HandleTypeDef *dev, uint8_t *uid_cl1_5bytes);
MFRC522_StatusTypeDef MFRC522_ReadUid4(MFRC522_HandleTypeDef *dev, uint8_t uid[4]);

#endif
