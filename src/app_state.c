#include "app_state.h"
#include "platform_hooks.h"
#include "app_config.h"
#include "board_pins.h"
#include "cesi_types.h"
#include "config_store.h"
#include "config_cli.h"
#include "led_status.h"
#include "button_events.h"
#include "acquisition.h"
#include "log_manager.h"
#include "grove_button.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "bme280.h"
#include <stdio.h>

static SystemMode  s_mode = MODE_STANDARD;
static SystemMode  s_prev_mode = MODE_STANDARD;   /* pour revenir au bon mode apres la maintenance */
static SystemError s_error = ERR_NONE;

static uint32_t s_last_measure = 0;
static bool     s_measure_due = true;
static bool     s_eco_gps_turn = true;
static uint32_t s_config_activity = 0;

void Background_Task(void)
{
    ButtonEvents_Update();
    Led_Refresh(s_mode, s_error);
}

static const char *mode_name(SystemMode m)
{
    switch (m)
    {
    case MODE_CONFIG:      return "CONFIGURATION";
    case MODE_ECONOMY:     return "ECONOMIQUE";
    case MODE_MAINTENANCE: return "MAINTENANCE";
    default:               return "STANDARD";
    }
}

static void enter_mode(SystemMode m)
{
    SystemMode old = s_mode;

    if (m == MODE_MAINTENANCE)
    {
        s_prev_mode = s_mode;
        Log_Suspend();                       /* on peut enlever la carte SD */
        printf("[SD] carte demontee, retrait possible\r\n");
    }
    else if (old == MODE_MAINTENANCE)
    {
        Log_Resume();                        /* la carte sera remontee toute seule */
    }

    s_mode = m;
    s_measure_due = true;
    s_eco_gps_turn = true;

    if (m == MODE_CONFIG)
    {
        s_error = ERR_NONE;
        s_config_activity = HAL_GetTick();
        printf("[MODE] %s - tapez HELP\r\n", mode_name(m));
    }
    else
    {
        printf("[MODE] %s\r\n", mode_name(m));
    }

    Led_Refresh(s_mode, s_error);
}

static uint32_t interval_ms(void)
{
    uint32_t i = (uint32_t)g_config.log_interval * LOG_INTERVAL_UNIT_MS;

    if (s_mode == MODE_ECONOMY)     i *= 2U;
    if (s_mode == MODE_MAINTENANCE) i = MAINTENANCE_PERIOD_MS;
    return i;
}

static void do_measurement(void)
{
    Measurement m;
    char line[128];
    bool gps = true;

    s_measure_due = false;
    s_last_measure = HAL_GetTick();

    if (s_mode == MODE_ECONOMY)
    {
        gps = s_eco_gps_turn;          /* en eco on lit le GPS une fois sur deux */
        s_eco_gps_turn = !s_eco_gps_turn;
    }

    s_error = Acquisition_Run(&m, gps);
    Acquisition_FormatLine(&m, line, sizeof(line));

    if (s_mode == MODE_MAINTENANCE)
        printf("[LIVE] %s\r\n", line);   /* en maintenance on affiche juste */
    else
    {
        Log_WriteLine(line);
        if (s_error == ERR_NONE) s_error = Log_GetError();   /* on regarde aussi si la carte SD a un probleme */
    }

    Led_Refresh(s_mode, s_error);
}

void App_Init(void)
{
    Config_Load();

    Led_Init();
    GroveButton_Init();
    HAL_Delay(100);

    bool red_at_boot = GroveButton2_IsPressed();   /* bouton rouge = bouton 2 */
    ButtonEvents_Init();

    printf("\r\n=== Worldwide Weather Watcher v%s (%s) ===\r\n", FW_VERSION, FW_BATCH);

    printf("RTC    : %s\r\n", GroveRTC_Init() ? "OK" : "ABSENTE");
    GroveGPS_Init();
    {
        uint8_t id = 0;
        bool ok = BME280_Init();
        BME280_ReadChipId(&id);
        printf("BME280 : %s (id=0x%02X)\r\n", ok ? "OK" : "ABSENT", id);
    }
    Log_Init();

#if APP_DEBUG_BUTTONS
    printf("TEST BOUTONS (vert=B1/PA9, rouge=B2/PC7), niveau brut :\r\n");
    while (1)
    {
        printf("B1=%d B2=%d\r\n",
               (int)HAL_GPIO_ReadPin(BUTTON1_PORT, BUTTON1_PIN),
               (int)HAL_GPIO_ReadPin(BUTTON2_PORT, BUTTON2_PIN));
        HAL_Delay(200);
    }
#endif

    if (red_at_boot) enter_mode(MODE_CONFIG);
    else             enter_mode(MODE_STANDARD);
}

void App_Step(void)
{
    Background_Task();

    ButtonEvent ev = ButtonEvents_Get();

    switch (s_mode)
    {
    case MODE_STANDARD:
        if (ev == BTN_EVT_GREEN_LONG)    enter_mode(MODE_ECONOMY);
        else if (ev == BTN_EVT_RED_LONG) enter_mode(MODE_MAINTENANCE);
        break;

    case MODE_ECONOMY:
#if ECO_EXIT_WITH_GREEN
        if (ev == BTN_EVT_GREEN_LONG)    enter_mode(MODE_STANDARD);
        else if (ev == BTN_EVT_RED_LONG) enter_mode(MODE_MAINTENANCE);
#else
        if (ev == BTN_EVT_RED_LONG)      enter_mode(MODE_STANDARD);
#endif
        break;

    case MODE_MAINTENANCE:
        if (ev == BTN_EVT_RED_LONG) enter_mode(s_prev_mode);
        break;

    case MODE_CONFIG:
        if (ConfigCli_Poll()) s_config_activity = HAL_GetTick();
        if ((HAL_GetTick() - s_config_activity) >= CONFIG_INACTIVITY_MS)
            enter_mode(MODE_STANDARD);
        return;   /* acquisition desactivee en configuration */
    }

    if (s_measure_due || (HAL_GetTick() - s_last_measure) >= interval_ms())
        do_measurement();
}
