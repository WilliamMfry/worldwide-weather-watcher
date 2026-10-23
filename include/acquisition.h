#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdbool.h>
#include <stddef.h>
#include "cesi_types.h"

typedef struct
{
    RTC_DateTime time;
    bool  time_ok;

    bool   gps_ok;        /* fix valide */
    double lat, lon;
    uint8_t sats;

    bool  press_ok;  float press;   /* hPa */
    bool  temp_ok;   float temp;    /* degC */
    bool  hum_ok;    float hum;     /* %    */
} Measurement;

/* lit tous les capteurs et renvoie l'erreur la plus importante */
SystemError Acquisition_Run(Measurement *m, bool read_gps);

/* met la mesure sous forme de ligne pour le fichier (NA si pas de valeur) */
void Acquisition_FormatLine(const Measurement *m, char *line, size_t len);

#endif
