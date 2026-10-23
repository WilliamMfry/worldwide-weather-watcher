#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "stm32f4xx_hal.h"

/* LED RGB branchee sur D6 */
#define RGB_DATA_PORT   GPIOA
#define RGB_DATA_PIN    GPIO_PIN_8
#define RGB_CLK_PORT    GPIOB
#define RGB_CLK_PIN     GPIO_PIN_10

/* double bouton branche sur D8
 * bouton 1 = vert, bouton 2 = rouge
 * si c'est inverse il suffit d'echanger les deux blocs */
#define BUTTON1_PORT GPIOA
#define BUTTON1_PIN  GPIO_PIN_9
#define BUTTON2_PORT GPIOC
#define BUTTON2_PIN  GPIO_PIN_7

/* 1 si le bouton donne 1 quand on appuie, sinon 0 */
#define BUTTON_ACTIVE_HIGH 0

/* GPS branche sur D2 (on fait que recevoir) */
#define GPS_UART_HANDLE huart1

/* I2C pour la RTC et le BME280 */
#define GROVE_I2C_HANDLE hi2c1

/* carte SD en SPI (CS sur D4) */
#define SD_CS_PORT      GPIOB
#define SD_CS_PIN       GPIO_PIN_5
#define SD_SPI_HANDLE   hspi1

#endif
