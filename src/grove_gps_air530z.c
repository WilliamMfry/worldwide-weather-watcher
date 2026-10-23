#include "grove_gps_air530z.h"
#include "board_pins.h"
#include "platform_hooks.h"
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef GPS_UART_HANDLE;

static double nmea_to_decimal(const char *value, char hemisphere)
{
    if (value == 0 || value[0] == '\0') return 0.0;

    double raw = atof(value);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (degrees * 100.0);
    double decimal = degrees + (minutes / 60.0);

    if (hemisphere == 'S' || hemisphere == 'W')
        decimal = -decimal;

    return decimal;
}

/* verifie le checksum de la trame */
static bool checksum_ok(const char *s)
{
    uint8_t cs = 0;
    const char *p = s + 1;

    while (*p && *p != '*')
        cs ^= (uint8_t)*p++;

    if (*p != '*') return false;
    return cs == (uint8_t)strtol(p + 1, 0, 16);
}

/* comme strtok mais garde les champs vides */
static int split_fields(char *s, char **fields, int max)
{
    int n = 0;
    fields[n++] = s;
    for (; *s && n < max; s++)
    {
        if (*s == ',')
        {
            *s = '\0';
            fields[n++] = s + 1;
        }
    }
    return n;
}

void GroveGPS_Init(void)
{
    /* on utilise que le RX, l'autre pin est prise par le bouton */
    GPS_UART_HANDLE.Instance = USART1;
    GPS_UART_HANDLE.Init.BaudRate = 9600;
    GPS_UART_HANDLE.Init.WordLength = UART_WORDLENGTH_8B;
    GPS_UART_HANDLE.Init.StopBits = UART_STOPBITS_1;
    GPS_UART_HANDLE.Init.Parity = UART_PARITY_NONE;
    GPS_UART_HANDLE.Init.Mode = UART_MODE_RX;
    GPS_UART_HANDLE.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    GPS_UART_HANDLE.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&GPS_UART_HANDLE);
}

bool GroveGPS_ReadLine(char *buffer, unsigned int max_len, unsigned int timeout_ms)
{
    if (buffer == 0 || max_len == 0) return false;

    unsigned int count = 0;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        Background_Task();

        if (__HAL_UART_GET_FLAG(&GPS_UART_HANDLE, UART_FLAG_RXNE))
        {
            char c = (char)(GPS_UART_HANDLE.Instance->DR & 0xFF);

            if (c == '\n')
            {
                buffer[count] = '\0';
                if (count > 0) return true;
                continue;
            }
            if (c == '\r') continue;
            if (count < max_len - 1) buffer[count++] = c;
        }
    }

    buffer[count] = '\0';
    return false;
}

bool GroveGPS_ReadGGA(GPS_Data *gps, unsigned int timeout_ms)
{
    if (gps == 0) return false;

    memset(gps, 0, sizeof(*gps));

    char line[96];
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        uint32_t elapsed = HAL_GetTick() - start;
        if (elapsed >= timeout_ms) break;

        if (!GroveGPS_ReadLine(line, sizeof(line), timeout_ms - elapsed))
            continue;

        char *s = strchr(line, '$');
        if (s == 0 || strlen(s) < 7) continue;
        if (strncmp(s + 3, "GGA", 3) != 0) continue;   /* on veut que les trames GGA */
        if (!checksum_ok(s)) continue;

        strncpy(gps->raw_sentence, s, sizeof(gps->raw_sentence) - 1);
        gps->raw_sentence[sizeof(gps->raw_sentence) - 1] = '\0';

        char *star = strchr(s, '*');
        if (star) *star = '\0';

        /* on recupere la position et le nombre de satellites */
        char *f[16] = {0};
        int n = split_fields(s, f, 16);
        if (n < 8) continue;

        gps->fix_quality = (uint8_t)atoi(f[6]);
        gps->satellites  = (uint8_t)atoi(f[7]);
        gps->valid = (gps->fix_quality > 0) && (f[2][0] != '\0') && (f[4][0] != '\0');

        if (gps->valid)
        {
            gps->latitude  = nmea_to_decimal(f[2], f[3][0]);
            gps->longitude = nmea_to_decimal(f[4], f[5][0]);
        }
        return true;
    }

    return false;
}
