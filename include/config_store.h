#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_PASSWORD_MAX  15
#define CONFIG_PASSWORD_MIN  4

typedef struct
{
    int32_t log_interval;     /* minutes */
    int32_t file_max_size;    /* octets  */
    int32_t timeout;          /* secondes */
    int32_t temp_air;
    int32_t min_temp_air;
    int32_t max_temp_air;
    int32_t hygr;
    int32_t hygr_mint;
    int32_t hygr_maxt;
    int32_t pressure;
    int32_t pressure_min;
    int32_t pressure_max;
    char    password[CONFIG_PASSWORD_MAX + 1];   /* mot de passe du mode configuration */
} Config;

extern Config g_config;

void Config_SetDefaults(Config *c);
void Config_Load(void);    /* charge la config (ou les valeurs par defaut) */
bool Config_Save(void);    /* sauvegarde dans la flash */
bool Config_Reset(void);   /* valeurs par defaut, SAUF le mot de passe */

#endif
