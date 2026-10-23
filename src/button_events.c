#include "button_events.h"
#include "grove_button.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

static uint32_t s_since[2];
static bool s_down[2];
static bool s_fired[2];
static ButtonEvent s_pending = BTN_EVT_NONE;

static bool is_pressed(int i)
{
    return (i == 0) ? GroveButton1_IsPressed() : GroveButton2_IsPressed();
}

void ButtonEvents_Init(void)
{
    /* si le bouton est deja appuye au demarrage on attend qu'il soit relache */
    for (int i = 0; i < 2; i++)
    {
        s_down[i] = false;
        s_fired[i] = is_pressed(i);
    }
    s_pending = BTN_EVT_NONE;
}

void ButtonEvents_Update(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < 2; i++)
    {
        if (is_pressed(i))
        {
            if (!s_down[i])
            {
                s_down[i] = true;
                s_since[i] = now;
            }
            else if (!s_fired[i] && (now - s_since[i]) >= LONG_PRESS_MS)
            {
                s_fired[i] = true;
                if (s_pending == BTN_EVT_NONE)
                    s_pending = (i == 0) ? BTN_EVT_GREEN_LONG : BTN_EVT_RED_LONG;
            }
        }
        else
        {
            s_down[i] = false;
            s_fired[i] = false;
        }
    }
}

ButtonEvent ButtonEvents_Get(void)
{
    ButtonEvent e = s_pending;
    s_pending = BTN_EVT_NONE;
    return e;
}
