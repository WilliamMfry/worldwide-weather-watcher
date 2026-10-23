#ifndef CONFIG_CLI_H
#define CONFIG_CLI_H

#include <stdbool.h>

/* lit ce qui arrive sur la console sans bloquer */
bool ConfigCli_Poll(void);
void ConfigCli_PrintHelp(void);

/* Verrouille la console : a appeler a chaque entree en mode configuration. */
void ConfigCli_Lock(void);

#endif
