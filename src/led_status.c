#include "led_status.h"
#include "grove_rgb.h"
#include "stm32f4xx_hal.h"

typedef struct { uint8_t r, g, b; } Rgb;

static const Rgb GREEN  = {0, 255, 0};
static const Rgb YELLOW = {255, 160, 0};
static const Rgb BLUE   = {0, 0, 255};
static const Rgb ORANGE = {255, 40, 0};
static const Rgb RED    = {255, 0, 0};
static const Rgb WHITE  = {255, 255, 255};

static Rgb s_last;
static bool s_valid = false;

void Led_Init(void)
{
    GroveRGB_Init();
    s_valid = false;
}

static Rgb mode_color(SystemMode m)
{
    switch (m)
    {
    case MODE_CONFIG:      return YELLOW;
    case MODE_ECONOMY:     return BLUE;
    case MODE_MAINTENANCE: return ORANGE;
    case MODE_STANDARD:
    default:               return GREEN;
    }
}

/* clignote entre deux couleurs, a peu pres une fois par seconde */
static Rgb blink(Rgb c1, uint32_t d1, Rgb c2)
{
    return (HAL_GetTick() % 1000U) < d1 ? c1 : c2;
}

void Led_Refresh(SystemMode mode, SystemError err)
{
    Rgb want;

    if (mode == MODE_CONFIG || err == ERR_NONE)
    {
        want = mode_color(mode);
    }
    else
    {
        switch (err)
        {
        case ERR_RTC:             want = blink(RED, 500, BLUE);   break;
        case ERR_GPS:             want = blink(RED, 500, YELLOW); break;
        case ERR_SENSOR_ACCESS:   want = blink(RED, 500, GREEN);  break;
        case ERR_SENSOR_INCOHERENT: want = blink(RED, 333, GREEN); break;  /* plus de vert */
        case ERR_SD_FULL:         want = blink(RED, 500, WHITE);  break;
        case ERR_SD_IO:           want = blink(RED, 333, WHITE);  break;  /* plus de blanc */
        default:                  want = mode_color(mode);        break;
        }
    }

    if (!s_valid || want.r != s_last.r || want.g != s_last.g || want.b != s_last.b)
    {
        GroveRGB_SetColor(want.r, want.g, want.b);
        s_last = want;
        s_valid = true;
    }
}
