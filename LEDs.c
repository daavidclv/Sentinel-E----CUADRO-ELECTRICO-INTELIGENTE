/*-----------------------------------------------------------------------------
 * Name:    Board_LEDF429.c
 * Purpose: LED interface .c file
 * Rev.:    1.0.0
 * Autor: 
 *----------------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "LEDs.h"

/* GPIO Pin identifier */
typedef struct _GPIO_PIN {
  GPIO_TypeDef *port;
  uint16_t      pin;
  uint16_t      reserved;
} GPIO_PIN;

/* LED GPIO Pins */
static const GPIO_PIN LED_PIN[] = {
//  { GPIOH, GPIO_PIN_3,  0U },
//  { GPIOH, GPIO_PIN_6,  0U },
//  { GPIOH, GPIO_PIN_7,  0U },
//  { GPIOI, GPIO_PIN_10, 0U },
//  { GPIOG, GPIO_PIN_6,  0U },
//  { GPIOG, GPIO_PIN_7,  0U },
//  { GPIOG, GPIO_PIN_8,  0U },
//  { GPIOH, GPIO_PIN_2,  0U }
  { GPIOB, GPIO_PIN_0,  0U },//CAMBIOS_LEDS
  { GPIOB, GPIO_PIN_7,  0U },
  { GPIOB, GPIO_PIN_14,  0U },
	{ GPIOD, GPIO_PIN_13,  0U },//CAMBIOS_RGB
  { GPIOD, GPIO_PIN_12,  0U },
  { GPIOD, GPIO_PIN_11,  0U }
};

#define LED_COUNT (sizeof(LED_PIN)/sizeof(GPIO_PIN))


/**
  \fn          int32_t LED_Initialize (void)
  \brief       Initialize LEDs
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t LED_Init (void) {
  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
//  __GPIOG_CLK_ENABLE();
//  __GPIOH_CLK_ENABLE();
//  __GPIOI_CLK_ENABLE();
    __GPIOB_CLK_ENABLE();//CAMBIO_Para LEDS
	  __GPIOD_CLK_ENABLE();//CAMBIO_ParaRGB

  /* Configure GPIO pins: PB0 PB7 PB14 *///CAMBIOS
  GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_14;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	  /* Configure GPIO pins: PD13 PD12 PD11 *///CAMBIOS
  GPIO_InitStruct.Pin   = GPIO_PIN_13 | GPIO_PIN_12 | GPIO_PIN_11;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	

//  /* Configure GPIO pins: PH2 PH3 PH6 PH7 */
//  GPIO_InitStruct.Pin   = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7;
//  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
//  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
//  GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
//  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

//  /* Configure GPIO pins: PI10 */
//  GPIO_InitStruct.Pin   = GPIO_PIN_10;
//  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
//  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
//  GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
//  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  return 0;
}

/**
  \fn          int32_t LED_UnInit (void)
  \brief       De-initialize LEDs
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t LED_UnInit (void) {

  HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_14);//LEDS 1,2y3
	HAL_GPIO_DeInit(GPIOD, GPIO_PIN_13 | GPIO_PIN_12 | GPIO_PIN_11);//RGB
//  HAL_GPIO_DeInit(GPIOH, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7);
//  HAL_GPIO_DeInit(GPIOI, GPIO_PIN_10);

  return 0;
}

/**
  \fn          int32_t LED_Encendido (uint32_t num)
  \brief       Turn on requested LED
  \param[in]   num  LED number
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t LED_Encendido (uint32_t num) {
  int32_t retCode = 0;

  if (num < LED_COUNT) {
		if(num<3)//Cambio para que RGB luzca con sus correposientes colores
    HAL_GPIO_WritePin(LED_PIN[num].port, LED_PIN[num].pin, GPIO_PIN_SET);
		else
    HAL_GPIO_WritePin(LED_PIN[num].port, LED_PIN[num].pin, GPIO_PIN_RESET);
  }
  else {
    retCode = -1;
  }

  return retCode;
}

/**
  \fn          int32_t LED_Apagado (uint32_t num)
  \brief       Turn off requested LED
  \param[in]   num  LED number
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t LED_Apagado (uint32_t num) {
  int32_t retCode = 0;

  if (num < LED_COUNT) {
		if(num<3)//Cambio para que RGB luzca con sus correposientes colores
    HAL_GPIO_WritePin(LED_PIN[num].port, LED_PIN[num].pin, GPIO_PIN_RESET);
		else
	  HAL_GPIO_WritePin(LED_PIN[num].port, LED_PIN[num].pin, GPIO_PIN_SET);
  }
  else {
    retCode = -1;
  }

  return retCode;
}

/**
  \fn          int32_t LED_Expo (uint32_t val)
  \brief       Write value to LEDs
  \param[in]   val  value to be displayed on LEDs
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t LED_Expo (uint32_t val) {
  uint32_t n;

  for (n = 0; n < LED_COUNT; n++) {
    if (val & (1 << n) ) 
			LED_Encendido (n);
    else                
			LED_Apagado(n);
  }

  return 0;
}

/**
  \fn          uint32_t LED_cnt (void)
  \brief       Get number of LEDs
  \return      Number of available LEDs
*/
uint32_t LED_cnt (void) {

  return LED_COUNT;
}
