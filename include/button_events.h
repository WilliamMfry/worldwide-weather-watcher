#ifndef BUTTON_EVENTS_H
#define BUTTON_EVENTS_H

typedef enum
{
    BTN_EVT_NONE = 0,
    BTN_EVT_GREEN_LONG,   /* appui 5 s sur le bouton vert  */
    BTN_EVT_RED_LONG      /* appui 5 s sur le bouton rouge */
} ButtonEvent;

void ButtonEvents_Init(void);
void ButtonEvents_Update(void);
ButtonEvent ButtonEvents_Get(void);   /* lit et efface l'evenement en attente */

#endif
