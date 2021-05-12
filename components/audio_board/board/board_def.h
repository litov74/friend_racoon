/**
 * *****************************************************************************
 * @file		board_def.h
 * @author		S. Naumov
 * *****************************************************************************
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef AUDIO_BOARD_DEF_H__
#define AUDIO_BOARD_DEF_H__

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <driver/gpio.h>

/* Export constants ----------------------------------------------------------*/

/**
 * @defgroup	board_pins Board pins
 * @{
 */

/* VS1053b */
#define PIN_NUM_VS1053B_SCLK	GPIO_NUM_18
#define PIN_NUM_VS1053B_SI		GPIO_NUM_23
#define PIN_NUM_VS1053B_SO		GPIO_NUM_19
#define PIN_NUM_VS1053B_DREQ	GPIO_NUM_35
#define PIN_NUM_VS1053B_XCS		GPIO_NUM_32
#define PIN_NUM_VS1053B_XDCS	GPIO_NUM_33
#define PIN_NUM_VS1053B_XRESET	GPIO_NUM_26
#define PIN_NUM_VS1053B_XMUTE	GPIO_NUM_21
/* PAM8403 */
#define PIN_NUM_AMP_XMUTE		GPIO_NUM_22
#define PIN_NUM_AMP_XSHDN		GPIO_NUM_4
/* MP45DT02 */
#define PIN_NUM_MP45DT02_LR		GPIO_NUM_5
#define PIN_NUM_MP45DT02_CLK	GPIO_NUM_14
#define PIN_NUM_MP45DT02_DOUT	GPIO_NUM_12
/* User button */
#define PIN_NUM_USER_BUTTON		GPIO_NUM_2
/* User LED */
#define PIN_NUM_USER_LED		GPIO_NUM_13

/** @}*/

/**
 * @defgroup	bit_masks Bit masks
 * @{
 */

/* VS1053b */
#define GPIO_VS1053B_DREQ_PIN_SEL	BIT64(PIN_NUM_VS1053B_DREQ)
#define GPIO_VS1053B_XRESET_PIN_SEL	BIT64(PIN_NUM_VS1053B_XRESET)
#define GPIO_VS1053B_XMUTE_PIN_SEL	BIT64(PIN_NUM_VS1053B_XMUTE)
/* PAM8403 */
#define GPIO_AMP_XMUTE_PIN_SEL		BIT64(PIN_NUM_AMP_XMUTE)
#define GPIO_AMP_XSHDN_PIN_SEL		BIT64(PIN_NUM_AMP_XSHDN)

/** @}*/

/* User-LED feature functionality */
#define BOARD_USER_LED_FEATURE	(1)	/*!< true or false */

#endif	/* AUDIO_BOARD_DEF_H__ */
