#ifndef GROVE_GPS_AIR530Z_H
#define GROVE_GPS_AIR530Z_H

#include <stdbool.h>
#include "cesi_types.h"

void GroveGPS_Init(void);
/* true si une ligne complete a ete recue avant le timeout */
bool GroveGPS_ReadLine(char *buffer, unsigned int max_len, unsigned int timeout_ms);
/* true si on a recu une trame GGA correcte */
bool GroveGPS_ReadGGA(GPS_Data *gps, unsigned int timeout_ms);

#endif
