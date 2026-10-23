#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdbool.h>
#include "cesi_types.h"

void Log_Init(void);                    /* monte la carte SD */
bool Log_WriteLine(const char *line);   /* ecrit la ligne dans la console et sur la carte */
SystemError Log_GetError(void);         /* erreur de la derniere ecriture */
void Log_Suspend(void);                 /* maintenance : demonte la carte */
void Log_Resume(void);

#endif
