#include "acquisition.h"
#include "config_store.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "bme280.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static uint8_t s_env_fail = 0;
static uint8_t s_gps_fail = 0;

static bool date_valid(const RTC_DateTime *t)
{
    return t->month >= 1 && t->month <= 12 &&
           t->day >= 1 && t->day <= 31 &&
           t->hours < 24 && t->minutes < 60 && t->seconds < 60;
}

SystemError Acquisition_Run(Measurement *m, bool read_gps)
{
    bool e_rtc = false, e_gps = false, e_acc = false, e_inc = false;
    uint32_t to_ms = (uint32_t)g_config.timeout * 1000UL;

    memset(m, 0, sizeof(*m));

    /* --- RTC --- */
    if (GroveRTC_GetDateTime(&m->time) && date_valid(&m->time))
        m->time_ok = true;
    else
        e_rtc = true;

    /* --- BME280 : pression / temperature / hygrometrie --- */
    if (g_config.temp_air || g_config.hygr || g_config.pressure)
    {
        Env_Data env;

        if (BME280_Read(&env, to_ms))
        {
            s_env_fail = 0;

            if (g_config.temp_air)
            {
                m->temp = env.temperature_c;
                m->temp_ok = true;
                if (env.temperature_c < g_config.min_temp_air || env.temperature_c > g_config.max_temp_air)
                    e_inc = true;
            }

            if (g_config.pressure)
            {
                m->press = env.pressure_hpa;
                m->press_ok = true;
                if (env.pressure_hpa < g_config.pressure_min || env.pressure_hpa > g_config.pressure_max)
                    e_inc = true;
            }

            if (g_config.hygr && env.has_humidity)
            {
                /* hors plage de temperature : mesure ignoree */
                if (env.temperature_c >= g_config.hygr_mint && env.temperature_c <= g_config.hygr_maxt)
                {
                    m->hum = env.humidity_percent;
                    m->hum_ok = true;
                }
            }
        }
        else
        {
            if (s_env_fail < 255) s_env_fail++;
            if (s_env_fail >= 2) e_acc = true;   /* erreur seulement si ca rate 2 fois de suite */
        }
    }

    /* --- GPS --- */
    if (read_gps)
    {
        GPS_Data gps;

        if (GroveGPS_ReadGGA(&gps, to_ms))
        {
            s_gps_fail = 0;
            if (gps.valid)
            {
                m->gps_ok = true;
                m->lat = gps.latitude;
                m->lon = gps.longitude;
                m->sats = gps.satellites;
            }
        }
        else
        {
            if (s_gps_fail < 255) s_gps_fail++;
            if (s_gps_fail >= 2) e_gps = true;
        }
    }
    else if (s_gps_fail >= 2)
    {
        e_gps = true;   /* on garde l'erreur meme quand on saute le GPS */
    }

    if (e_rtc) return ERR_RTC;
    if (e_gps) return ERR_GPS;
    if (e_acc) return ERR_SENSOR_ACCESS;
    if (e_inc) return ERR_SENSOR_INCOHERENT;
    return ERR_NONE;
}

static void put(char *buf, size_t len, size_t *pos, const char *fmt, ...)
{
    if (*pos >= len) return;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *pos, len - *pos, fmt, ap);
    va_end(ap);

    if (n > 0) *pos += (size_t)n;
    if (*pos >= len) *pos = len - 1;
}

void Acquisition_FormatLine(const Measurement *m, char *line, size_t len)
{
    size_t p = 0;
    line[0] = '\0';

    if (m->time_ok)
        put(line, len, &p, "%04u-%02u-%02u %02u:%02u:%02u",
            (unsigned)m->time.year, (unsigned)m->time.month, (unsigned)m->time.day,
            (unsigned)m->time.hours, (unsigned)m->time.minutes, (unsigned)m->time.seconds);
    else
        put(line, len, &p, "NA");

    if (m->gps_ok) put(line, len, &p, ";%.5f;%.5f", m->lat, m->lon);
    else           put(line, len, &p, ";NA;NA");

    if (m->press_ok) put(line, len, &p, ";%.1f", (double)m->press); else put(line, len, &p, ";NA");
    if (m->temp_ok)  put(line, len, &p, ";%.1f", (double)m->temp);  else put(line, len, &p, ";NA");
    if (m->hum_ok)   put(line, len, &p, ";%.1f", (double)m->hum);   else put(line, len, &p, ";NA");
}
