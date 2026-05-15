/*-----------------------------------------------------------------------------
 * Name:    Board_LEDF429.h
 * Purpose: LED interface header file
 * Rev.:    1.0.0
 * Autor: 
 *----------------------------------------------------------------------------*/

#ifndef __BOARD_LEDF429_H
#define __BOARD_LEDF429_H

#include <stdint.h>

/**
  \fn          int32_t LED_Init (void)
  \brief       Initialize I/O interface for LEDs
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
/**
  \fn          int32_t LED_UnInit (void)
  \brief       De-initialize I/O interface for LEDs
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
/**
  \fn          int32_t LED_Encendido (uint32_t num)
  \brief       Turn on a single LED indicated by \em num
  \param[in]   num  LED number
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
/**
  \fn          int32_t LED_Apagado (uint32_t num)
  \brief       Turn off a single LED indicated by \em num
  \param[in]   num  LED number
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
/**
  \fn          int32_t LED_Expo (uint32_t val)
  \brief       Control all LEDs with the bit vector \em val
  \param[in]   val  each bit represents the status of one LED.
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
/**
  \fn          uint32_t LED_cnt (void)
  \brief       Get number of available LEDs on evaluation hardware
  \return      Number of available LEDs
*/

extern int32_t  LED_Init   (void);
extern int32_t  LED_UnInit (void);
extern int32_t  LED_Encendido           (uint32_t num);
extern int32_t  LED_Apagado          (uint32_t num);
extern int32_t  LED_Expo       (uint32_t val);
extern uint32_t LED_cnt     (void);

#endif /* __BOARD_LEDF429_H */
