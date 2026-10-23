#include "config_cli.h"
#include "config_store.h"
#include "grove_rtc_ds1307.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern UART_HandleTypeDef huart2;

#define MAX_FAILS  3
#define LOCK_MS    30000UL

typedef struct
{
    const char *name;
    int32_t *value;
    int32_t min;
    int32_t max;
} Param;

static const Param s_params[] = {
    {"LOG_INTERVAL", &g_config.log_interval,  1,    1440},
    {"FILE_MAX_SIZE",&g_config.file_max_size, 512,  65536},
    {"TIMEOUT",      &g_config.timeout,       1,    600},
    {"TEMP_AIR",     &g_config.temp_air,      0,    1},
    {"MIN_TEMP_AIR", &g_config.min_temp_air,  -40,  85},
    {"MAX_TEMP_AIR", &g_config.max_temp_air,  -40,  85},
    {"HYGR",         &g_config.hygr,          0,    1},
    {"HYGR_MINT",    &g_config.hygr_mint,     -40,  85},
    {"HYGR_MAXT",    &g_config.hygr_maxt,     -40,  85},
    {"PRESSURE",     &g_config.pressure,      0,    1},
    {"PRESSURE_MIN", &g_config.pressure_min,  300,  1100},
    {"PRESSURE_MAX", &g_config.pressure_max,  300,  1100},
};
#define PARAM_COUNT (sizeof(s_params) / sizeof(s_params[0]))

static char s_buf[96];
static unsigned s_len = 0;

static bool s_auth = false;
static uint8_t s_fails = 0;
static uint32_t s_locked_until = 0;

static char *strip(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
    return s;
}

static void upper(char *s)
{
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* compare deux noms sans tenir compte des _ */
static bool name_eq(const char *a, const char *b)
{
    for (;;)
    {
        while (*a == '_') a++;
        while (*b == '_') b++;
        if (*a != *b) return false;
        if (*a == '\0') return true;
        a++;
        b++;
    }
}

static bool parse_long(const char *s, long *out, const char **next)
{
    char *end;
    if (*s == '\0') return false;
    long v = strtol(s, &end, 10);
    if (end == s) return false;
    *out = v;
    *next = end;
    return true;
}

/* lit 3 nombres separes par un caractere */
static bool parse_triplet(const char *s, char sep, long *a, long *b, long *c)
{
    const char *p;
    if (!parse_long(s, a, &p) || *p != sep) return false;
    if (!parse_long(p + 1, b, &p) || *p != sep) return false;
    if (!parse_long(p + 1, c, &p) || *p != '\0') return false;
    return true;
}

static void save_and_report(void)
{
    if (Config_Save()) printf("OK\r\n");
    else               printf("ERREUR: ecriture Flash\r\n");
}

static void rtc_get_or_zero(RTC_DateTime *dt)
{
    if (!GroveRTC_GetDateTime(dt))
    {
        memset(dt, 0, sizeof(*dt));
        dt->day = 1; dt->month = 1; dt->year = 2000; dt->dayOfWeek = 1;
    }
}

static void cmd_clock(const char *val)
{
    long h, m, s;
    if (!parse_triplet(val, ':', &h, &m, &s) || h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59)
    {
        printf("ERREUR: format CLOCK=HH:MM:SS\r\n");
        return;
    }
    RTC_DateTime dt;
    rtc_get_or_zero(&dt);
    dt.hours = (uint8_t)h; dt.minutes = (uint8_t)m; dt.seconds = (uint8_t)s;
    printf(GroveRTC_SetDateTime(&dt) ? "OK\r\n" : "ERREUR: acces RTC\r\n");
}

static void cmd_date(const char *val)
{
    long mo, d, y;
    if (!parse_triplet(val, ',', &mo, &d, &y) || mo < 1 || mo > 12 || d < 1 || d > 31 || y < 2000 || y > 2099)
    {
        printf("ERREUR: format DATE=MOIS,JOUR,ANNEE\r\n");
        return;
    }
    RTC_DateTime dt;
    rtc_get_or_zero(&dt);
    dt.month = (uint8_t)mo; dt.day = (uint8_t)d; dt.year = (uint16_t)y;
    printf(GroveRTC_SetDateTime(&dt) ? "OK\r\n" : "ERREUR: acces RTC\r\n");
}

static void cmd_day(char *val)
{
    static const char *days[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
    upper(val);
    for (int i = 0; i < 7; i++)
    {
        if (strcmp(val, days[i]) == 0)
        {
            RTC_DateTime dt;
            rtc_get_or_zero(&dt);
            dt.dayOfWeek = (uint8_t)(i + 1);
            printf(GroveRTC_SetDateTime(&dt) ? "OK\r\n" : "ERREUR: acces RTC\r\n");
            return;
        }
    }
    printf("ERREUR: DAY=MON|TUE|WED|THU|FRI|SAT|SUN\r\n");
}

static void cmd_show(void)
{
    for (unsigned i = 0; i < PARAM_COUNT; i++)
        printf("%s=%ld\r\n", s_params[i].name, (long)*s_params[i].value);

    RTC_DateTime dt;
    if (GroveRTC_GetDateTime(&dt))
        printf("RTC: %04u-%02u-%02u %02u:%02u:%02u (jour %u)\r\n",
               (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day,
               (unsigned)dt.hours, (unsigned)dt.minutes, (unsigned)dt.seconds, (unsigned)dt.dayOfWeek);
    else
        printf("RTC: inaccessible\r\n");
}

void ConfigCli_PrintHelp(void)
{
    printf("Commandes : PARAM=valeur | RESET | VERSION | SHOW | HELP | LOGOUT\r\n");
    printf("  LOG_INTERVAL FILE_MAX_SIZE TIMEOUT TEMP_AIR MIN_TEMP_AIR MAX_TEMP_AIR\r\n");
    printf("  HYGR HYGR_MINT HYGR_MAXT PRESSURE PRESSURE_MIN PRESSURE_MAX\r\n");
    printf("  CLOCK=HH:MM:SS  DATE=MOIS,JOUR,ANNEE  DAY=MON|TUE|WED|THU|FRI|SAT|SUN\r\n");
    printf("  NEWPASSWORD=xxxx (4 a %d caracteres)\r\n", CONFIG_PASSWORD_MAX);
}

/* ---- mot de passe ---- */

void ConfigCli_Lock(void)
{
    s_auth = false;
    s_len = 0;
    printf("Acces protege : tapez PASSWORD=<mot de passe>\r\n");
}

static void cmd_login(const char *val)
{
    uint32_t now = HAL_GetTick();

    if ((int32_t)(s_locked_until - now) > 0)
    {
        printf("Verrouille encore %lu s\r\n", (unsigned long)((s_locked_until - now) / 1000U + 1U));
        return;
    }

    if (strcmp(val, g_config.password) == 0)
    {
        s_auth = true;
        s_fails = 0;
        printf("OK, acces autorise. Tapez HELP\r\n");
        return;
    }

    s_fails++;
    if (s_fails >= MAX_FAILS)
    {
        s_fails = 0;
        s_locked_until = now + LOCK_MS;
        printf("MOT DE PASSE INCORRECT - verrouille 30 s\r\n");
    }
    else
    {
        printf("MOT DE PASSE INCORRECT (%u/%u)\r\n", (unsigned)s_fails, (unsigned)MAX_FAILS);
    }
}

static void cmd_newpass(const char *val)
{
    size_t n = strlen(val);

    if (n < CONFIG_PASSWORD_MIN || n > CONFIG_PASSWORD_MAX)
    {
        printf("ERREUR: le mot de passe doit faire %d a %d caracteres\r\n", CONFIG_PASSWORD_MIN, CONFIG_PASSWORD_MAX);
        return;
    }
    for (size_t i = 0; i < n; i++)
    {
        if (!isgraph((unsigned char)val[i]))
        {
            printf("ERREUR: pas d'espace ni de caractere special invisible\r\n");
            return;
        }
    }

    memset(g_config.password, 0, sizeof(g_config.password));
    strcpy(g_config.password, val);
    save_and_report();
}

static void handle_line(char *line)
{
    line = strip(line);
    if (*line == '\0') return;

    char *eq = strchr(line, '=');
    char *name = line;
    char *val = line + strlen(line);   /* chaine vide par defaut */

    if (eq)
    {
        *eq = '\0';
        val = strip(eq + 1);
        name = strip(line);
    }
    upper(name);   /* on met juste le nom en majuscules (pas le mot de passe) */

    /* pas connecte = on accepte que PASSWORD */
    if (!s_auth)
    {
        if (eq && strcmp(name, "PASSWORD") == 0)
            cmd_login(val);
        else
            printf("ACCES REFUSE : tapez PASSWORD=<mot de passe>\r\n");
        return;
    }

    if (eq == 0)
    {
        if (strcmp(name, "RESET") == 0)
        {
            printf(Config_Reset() ? "OK (valeurs par defaut, mot de passe conserve)\r\n" : "ERREUR: ecriture Flash\r\n");
        }
        else if (strcmp(name, "VERSION") == 0)
        {
            printf("VERSION %s LOT %s\r\n", FW_VERSION, FW_BATCH);
        }
        else if (strcmp(name, "SHOW") == 0)   cmd_show();
        else if (strcmp(name, "HELP") == 0)   ConfigCli_PrintHelp();
        else if (strcmp(name, "LOGOUT") == 0) { s_auth = false; printf("Console verrouillee\r\n"); }
        else printf("ERREUR: commande inconnue (HELP)\r\n");
        return;
    }

    if (strcmp(name, "PASSWORD") == 0)    { printf("Deja connecte\r\n"); return; }
    if (strcmp(name, "NEWPASSWORD") == 0) { cmd_newpass(val); return; }
    if (strcmp(name, "CLOCK") == 0)       { cmd_clock(val);   return; }
    if (strcmp(name, "DATE") == 0)        { cmd_date(val);    return; }
    if (strcmp(name, "DAY") == 0)         { cmd_day(val);     return; }

    if (name_eq(name, "LOG_INTERVALL")) name = "LOG_INTERVAL";   /* au cas ou on l'ecrit avec deux L */

    for (unsigned i = 0; i < PARAM_COUNT; i++)
    {
        if (!name_eq(name, s_params[i].name)) continue;

        long v;
        const char *end;
        if (!parse_long(val, &v, &end) || *end != '\0' || v < s_params[i].min || v > s_params[i].max)
        {
            printf("ERREUR: %s doit etre dans [%ld ; %ld]\r\n", s_params[i].name,
                   (long)s_params[i].min, (long)s_params[i].max);
            return;
        }
        *s_params[i].value = (int32_t)v;
        save_and_report();
        return;
    }

    printf("ERREUR: parametre inconnu (HELP)\r\n");
}

bool ConfigCli_Poll(void)
{
    bool activity = false;

    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
    {
        char c = (char)(huart2.Instance->DR & 0xFF);
        activity = true;

        if (c == '\r' || c == '\n')
        {
            if (s_len > 0)
            {
                s_buf[s_len] = '\0';
                s_len = 0;
                handle_line(s_buf);
            }
        }
        else if (s_len < sizeof(s_buf) - 1)
        {
            s_buf[s_len++] = c;
        }
    }
    return activity;
}
