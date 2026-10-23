#ifndef BME280_H
#define BME280_H

#include <stdbool.h>
#include <stdint.h>
#include "cesi_types.h"

bool BME280_Init(void);
bool BME280_ReadChipId(uint8_t *id);                       /* pour savoir si c'est un BME ou un BMP */
bool BME280_Read(Env_Data *data, uint32_t timeout_ms);

#endif
