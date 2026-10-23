#include "log_manager.h"
#include "config_store.h"
#include "grove_rtc_ds1307.h"
#include "sd_logger.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

static FATFS s_fs;
static bool s_mounted = false;
static bool s_suspended = false;
static SystemError s_err = ERR_NONE;

static bool mount_card(void)
{
    if (s_mounted) return true;

    FRESULT fr = f_mount(&s_fs, "", 1);
    if (fr == FR_OK)
    {
        s_mounted = true;
        return true;
    }

    printf("[SD] montage impossible (FatFs=%d, init=%d)\r\n", (int)fr, (int)SDLogger_GetLastInitStatus());
    return false;
}

static void unmount_card(void)
{
    (void)f_mount(NULL, "", 0);
    s_mounted = false;
}

/* nom du fichier a partir de la date */
static void date_base(char *b, size_t n)
{
    RTC_DateTime t;

    if (GroveRTC_GetDateTime(&t) && t.year >= 2000 && t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31)
        snprintf(b, n, "%02u%02u%02u", (unsigned)(t.year - 2000), (unsigned)t.month, (unsigned)t.day);
    else
        snprintf(b, n, "NODATE");
}

static void make_name(char *out, size_t n, const char *base, unsigned rev)
{
    snprintf(out, n, "%s_%u.LOG", base, rev);
}

/* renomme le fichier plein pour en commencer un nouveau */
static FRESULT rotate(const char *base)
{
    char cur[24], dst[24];
    FILINFO fi;

    make_name(cur, sizeof(cur), base, 0);

    for (unsigned rev = 1; rev < 1000; rev++)
    {
        make_name(dst, sizeof(dst), base, rev);
        FRESULT fr = f_stat(dst, &fi);
        if (fr == FR_NO_FILE) return f_rename(cur, dst);
        if (fr != FR_OK) return fr;
    }
    return FR_DENIED;
}

void Log_Init(void)
{
    bool ok = mount_card();
    printf("SD     : %s\r\n", ok ? "OK" : "ABSENTE ou illisible (FAT32 ?)");
}

bool Log_WriteLine(const char *line)
{
    char buf[160];
    char base[8], name[24];
    FIL f;
    FRESULT fr;
    UINT bw = 0;
    bool full = false;

    printf("[LOG] %s\r\n", line);

    s_err = ERR_NONE;
    if (s_suspended) return false;

    if (!mount_card())
    {
        s_err = ERR_SD_IO;
        return false;
    }

    int n = snprintf(buf, sizeof(buf), "%s\r\n", line);
    if (n < 0 || n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;

    date_base(base, sizeof(base));
    make_name(name, sizeof(name), base, 0);

    fr = f_open(&f, name, FA_WRITE | FA_OPEN_APPEND);

    if (fr == FR_OK && f_size(&f) > 0 && (f_size(&f) + (FSIZE_t)n) > (FSIZE_t)g_config.file_max_size)
    {
        /* fichier trop gros : on l'archive et on en recommence un */
        f_close(&f);
        fr = rotate(base);
        if (fr == FR_OK) fr = f_open(&f, name, FA_WRITE | FA_OPEN_APPEND);
    }

    if (fr == FR_OK)
    {
        fr = f_write(&f, buf, (UINT)n, &bw);
        FRESULT fc = f_close(&f);          /* sinon les donnees sont pas vraiment ecrites */
        if (fr == FR_OK) fr = fc;
        if (fr == FR_OK && bw != (UINT)n) full = true;   /* tout n'a pas ete ecrit donc la carte est pleine */
    }

    if (full || fr == FR_DENIED)
    {
        s_err = ERR_SD_FULL;
    }
    else if (fr != FR_OK)
    {
        s_err = ERR_SD_IO;
        unmount_card();                    /* on reessaiera la prochaine fois */
    }

    if (s_err != ERR_NONE)
        printf("[SD] erreur d'ecriture (%s, FatFs=%d)\r\n", full ? "plein" : "E/S", (int)fr);

    return s_err == ERR_NONE;
}

SystemError Log_GetError(void)
{
    return s_err;
}

void Log_Suspend(void)
{
    unmount_card();
    s_suspended = true;
}

void Log_Resume(void)
{
    s_suspended = false;
}
