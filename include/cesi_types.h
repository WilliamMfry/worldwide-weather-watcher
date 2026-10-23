#ifndef CESI_TYPES_H
#define CESI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t dayOfWeek;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} RTC_DateTime;

typedef struct
{
    bool valid;              /* true si fix GPS */
    char raw_sentence[96];
    double latitude;
    double longitude;
    uint8_t fix_quality;
    uint8_t satellites;
} GPS_Data;

typedef struct
{
    float temperature_c;
    float humidity_percent;
    float pressure_hpa;
    bool  has_humidity;      /* false si le composant est un BMP280 */
} Env_Data;

typedef enum
{
    MODE_STANDARD = 0,
    MODE_CONFIG,
    MODE_ECONOMY,
    MODE_MAINTENANCE
} SystemMode;

typedef enum
{
    ERR_NONE = 0,
    ERR_RTC,
    ERR_GPS,
    ERR_SENSOR_ACCESS,
    ERR_SENSOR_INCOHERENT,
    ERR_SD_FULL,
    ERR_SD_IO
} SystemError;

#endif
