#include "grove_button.h"
#include "board_pins.h"

#if BUTTON_ACTIVE_HIGH
  #define BTN_PULL   GPIO_PULLDOWN
  #define BTN_ACTIVE GPIO_PIN_SET
#else
  #define BTN_PULL   GPIO_PULLUP
  #define BTN_ACTIVE GPIO_PIN_RESET
#endif

void GroveButton_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = BTN_PULL;

    gpio.Pin = BUTTON1_PIN;
    HAL_GPIO_Init(BUTTON1_PORT, &gpio);

    gpio.Pin = BUTTON2_PIN;
    HAL_GPIO_Init(BUTTON2_PORT, &gpio);
}

bool GroveButton1_IsPressed(void)
{
    return HAL_GPIO_ReadPin(BUTTON1_PORT, BUTTON1_PIN) == BTN_ACTIVE;
}

bool GroveButton2_IsPressed(void)
{
    return HAL_GPIO_ReadPin(BUTTON2_PORT, BUTTON2_PIN) == BTN_ACTIVE;
}
