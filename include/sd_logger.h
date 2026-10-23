#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SDLOGGER_CARD_UNKNOWN = 0,
    SDLOGGER_CARD_SDSC,
    SDLOGGER_CARD_SDHC
} SDLoggerCardType;

typedef enum
{
    SDLOGGER_INIT_OK = 0,
    SDLOGGER_INIT_ERR_CMD0,      /* la carte ne repond pas : cablage / carte absente */
    SDLOGGER_INIT_ERR_CMD8,
    SDLOGGER_INIT_ERR_ACMD41,
    SDLOGGER_INIT_ERR_CMD58,
    SDLOGGER_INIT_ERR_CMD16
} SDLoggerInitStatus;

/* fonctions de base pour lire/ecrire sur la carte (utilisees par FatFs) */
bool SDLogger_Init(void);
bool SDLogger_ReadBlock(uint8_t *buf, uint32_t sector);
bool SDLogger_WriteBlock(const uint8_t *buf, uint32_t sector);
bool SDLogger_Sync(void);

SDLoggerCardType SDLogger_GetCardType(void);
SDLoggerInitStatus SDLogger_GetLastInitStatus(void);

#endif
