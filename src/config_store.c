#include "config_store.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* pas d'EEPROM sur la carte donc on sauvegarde dans la flash */
#define CONFIG_FLASH_ADDR  0x08060000UL
#define CONFIG_MAGIC       0x57575703UL   /* a changer si on modifie la structure Config */

typedef struct
{
    uint32_t magic;
    Config cfg;
    uint32_t checksum;
} StoredConfig;

Config g_config;

static uint32_t calc_checksum(const Config *c)
{
    const uint32_t *w = (const uint32_t *)c;
    uint32_t s = 0x1234ABCDUL;
    for (unsigned i = 0; i < sizeof(Config) / 4; i++)
        s = ((s << 5) | (s >> 27)) ^ w[i];
    return s;
}

void Config_SetDefaults(Config *c)
{
    c->log_interval  = 10;
    c->file_max_size = 2048;
    c->timeout       = 30;
    c->temp_air      = 1;
    c->min_temp_air  = -10;
    c->max_temp_air  = 60;
    c->hygr          = 1;
    c->hygr_mint     = 0;
    c->hygr_maxt     = 50;
    c->pressure      = 1;
    c->pressure_min  = 850;
    c->pressure_max  = 1080;

    memset(c->password, 0, sizeof(c->password));
    strcpy(c->password, "1234");
}

void Config_Load(void)
{
    const StoredConfig *st = (const StoredConfig *)CONFIG_FLASH_ADDR;

    if (st->magic == CONFIG_MAGIC && st->checksum == calc_checksum(&st->cfg))
    {
        g_config = st->cfg;
        g_config.password[CONFIG_PASSWORD_MAX] = '\0';
    }
    else
    {
        Config_SetDefaults(&g_config);
    }
}

bool Config_Save(void)
{
    StoredConfig st;
    st.magic = CONFIG_MAGIC;
    st.cfg = g_config;
    st.checksum = calc_checksum(&g_config);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    FLASH_EraseInitTypeDef er = {0};
    uint32_t sector_error = 0;
    er.TypeErase    = FLASH_TYPEERASE_SECTORS;
    er.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    er.Sector       = FLASH_SECTOR_7;
    er.NbSectors    = 1;

    bool ok = (HAL_FLASHEx_Erase(&er, &sector_error) == HAL_OK);

    if (ok)
    {
        const uint32_t *w = (const uint32_t *)&st;
        for (unsigned i = 0; i < sizeof(st) / 4 && ok; i++)
            ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, CONFIG_FLASH_ADDR + 4U * i, w[i]) == HAL_OK);
    }

    HAL_FLASH_Lock();
    return ok;
}

bool Config_Reset(void)
{
    char pw[CONFIG_PASSWORD_MAX + 1];
    memcpy(pw, g_config.password, sizeof(pw));

    Config_SetDefaults(&g_config);
    memcpy(g_config.password, pw, sizeof(pw));   /* on garde le mot de passe */

    return Config_Save();
}
