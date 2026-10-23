#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "cesi_types.h"

void Led_Init(void);
/* A appeler souvent : affiche la couleur du mode, ou le clignotement de l'erreur. */
void Led_Refresh(SystemMode mode, SystemError err);

#endif
