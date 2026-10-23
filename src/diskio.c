/* fait le lien entre FatFs et notre code pour la carte SD */
#include "ff.h"
#include "diskio.h"
#include "sd_logger.h"
#include "grove_rtc_ds1307.h"

static DSTATUS s_stat = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv)
{
    return pdrv ? STA_NOINIT : s_stat;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv) return STA_NOINIT;
    s_stat = SDLogger_Init() ? 0 : STA_NOINIT;
    return s_stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv || count == 0) return RES_PARERR;
    if (s_stat & STA_NOINIT) return RES_NOTRDY;

    while (count--)
    {
        if (!SDLogger_ReadBlock(buff, sector)) return RES_ERROR;
        buff += 512;
        sector++;
    }
    return RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv || count == 0) return RES_PARERR;
    if (s_stat & STA_NOINIT) return RES_NOTRDY;

    while (count--)
    {
        if (!SDLogger_WriteBlock(buff, sector)) return RES_ERROR;
        buff += 512;
        sector++;
    }
    return RES_OK;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv) return RES_PARERR;
    if (s_stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd)
    {
    case CTRL_SYNC:
        return SDLogger_Sync() ? RES_OK : RES_ERROR;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

/* pour mettre la bonne date sur les fichiers */
DWORD get_fattime(void)
{
    RTC_DateTime t;

    if (!GroveRTC_GetDateTime(&t) || t.year < 1980 || t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31)
        return ((DWORD)(2000 - 1980) << 25) | (1UL << 21) | (1UL << 16);

    return ((DWORD)(t.year - 1980) << 25) | ((DWORD)t.month << 21) | ((DWORD)t.day << 16) |
           ((DWORD)t.hours << 11) | ((DWORD)t.minutes << 5) | ((DWORD)(t.seconds / 2));
}
