#ifndef PLATFORM_HOOKS_H
#define PLATFORM_HOOKS_H

/* Appelee par les boucles d'attente (ex. lecture GPS) pour garder LED et boutons vivants. */
void Background_Task(void);

#endif
