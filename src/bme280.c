#include "bme280.h"
#include "board_pins.h"

extern I2C_HandleTypeDef GROVE_I2C_HANDLE;

#define REG_ID        0xD0
#define REG_RESET     0xE0
#define REG_CTRL_HUM  0xF2
#define REG_STATUS    0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG    0xF5
#define REG_DATA      0xF7

static uint16_t s_addr = (0x76 << 1);
static bool s_ready = false;
static bool s_has_hum = false;

static struct
{
    uint16_t T1; int16_t T2, T3;
    uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;
    uint8_t  H1; int16_t H2; uint8_t H3; int16_t H4, H5; int8_t H6;
} cal;

static bool rd(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&GROVE_I2C_HANDLE, s_addr, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100) == HAL_OK;
}

static bool wr(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&GROVE_I2C_HANDLE, s_addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100) == HAL_OK;
}

bool BME280_ReadChipId(uint8_t *id)
{
    if (id == 0) return false;
    return rd(REG_ID, id, 1);
}

static bool read_calibration(void)
{
    uint8_t b[24];
    uint8_t h1;
    uint8_t e[7];

    if (!rd(0x88, b, 24)) return false;

    cal.T1 = (uint16_t)(b[1] << 8 | b[0]);
    cal.T2 = (int16_t)(b[3] << 8 | b[2]);
    cal.T3 = (int16_t)(b[5] << 8 | b[4]);
    cal.P1 = (uint16_t)(b[7] << 8 | b[6]);
    cal.P2 = (int16_t)(b[9] << 8 | b[8]);
    cal.P3 = (int16_t)(b[11] << 8 | b[10]);
    cal.P4 = (int16_t)(b[13] << 8 | b[12]);
    cal.P5 = (int16_t)(b[15] << 8 | b[14]);
    cal.P6 = (int16_t)(b[17] << 8 | b[16]);
    cal.P7 = (int16_t)(b[19] << 8 | b[18]);
    cal.P8 = (int16_t)(b[21] << 8 | b[20]);
    cal.P9 = (int16_t)(b[23] << 8 | b[22]);

    if (s_has_hum)
    {
        if (!rd(0xA1, &h1, 1)) return false;
        if (!rd(0xE1, e, 7)) return false;

        cal.H1 = h1;
        cal.H2 = (int16_t)(e[1] << 8 | e[0]);
        cal.H3 = e[2];
        cal.H4 = (int16_t)(((int16_t)(int8_t)e[3] << 4) | (e[4] & 0x0F));
        cal.H5 = (int16_t)(((int16_t)(int8_t)e[5] << 4) | (e[4] >> 4));
        cal.H6 = (int8_t)e[6];
    }
    return true;
}

bool BME280_Init(void)
{
    static const uint8_t addrs[2] = {0x76, 0x77};
    uint8_t id = 0;
    bool found = false;

    s_ready = false;

    for (int i = 0; i < 2 && !found; i++)
    {
        s_addr = (uint16_t)(addrs[i] << 1);
        if (BME280_ReadChipId(&id) && (id == 0x60 || id == 0x58))
            found = true;
    }
    if (!found) return false;

    s_has_hum = (id == 0x60);

    (void)wr(REG_RESET, 0xB6);
    HAL_Delay(10);

    if (!read_calibration()) return false;

    s_ready = true;
    return true;
}

bool BME280_Read(Env_Data *d, uint32_t timeout_ms)
{
    uint8_t raw[8];
    uint8_t st = 0;

    if (d == 0) return false;
    if (!s_ready && !BME280_Init()) return false;

    /* on lance une seule mesure */
    if (s_has_hum && !wr(REG_CTRL_HUM, 0x01)) goto fail;
    if (!wr(REG_CONFIG, 0x00)) goto fail;
    if (!wr(REG_CTRL_MEAS, 0x25)) goto fail;

    HAL_Delay(15);

    uint32_t start = HAL_GetTick();
    do
    {
        if (!rd(REG_STATUS, &st, 1)) goto fail;
        if ((st & 0x08) == 0) break;
        HAL_Delay(2);
    } while ((HAL_GetTick() - start) < timeout_ms);

    if (st & 0x08) goto fail;   /* timeout */

    if (!rd(REG_DATA, raw, 8)) goto fail;

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    int32_t adc_H = ((int32_t)raw[6] << 8) | raw[7];

    if (adc_T == 0x80000 || adc_P == 0x80000) goto fail;

    /* calculs repris de la datasheet */
    double var1 = ((double)adc_T / 16384.0 - (double)cal.T1 / 1024.0) * (double)cal.T2;
    double var2 = (((double)adc_T / 131072.0 - (double)cal.T1 / 8192.0) *
                   ((double)adc_T / 131072.0 - (double)cal.T1 / 8192.0)) * (double)cal.T3;
    double t_fine = var1 + var2;
    double temp = t_fine / 5120.0;

    double p1 = t_fine / 2.0 - 64000.0;
    double p2 = p1 * p1 * (double)cal.P6 / 32768.0;
    p2 = p2 + p1 * (double)cal.P5 * 2.0;
    p2 = p2 / 4.0 + (double)cal.P4 * 65536.0;
    p1 = ((double)cal.P3 * p1 * p1 / 524288.0 + (double)cal.P2 * p1) / 524288.0;
    p1 = (1.0 + p1 / 32768.0) * (double)cal.P1;

    double press = 0.0;
    if (p1 != 0.0)
    {
        double p = 1048576.0 - (double)adc_P;
        p = (p - p2 / 4096.0) * 6250.0 / p1;
        p1 = (double)cal.P9 * p * p / 2147483648.0;
        p2 = p * (double)cal.P8 / 32768.0;
        p = p + (p1 + p2 + (double)cal.P7) / 16.0;
        press = p / 100.0;   /* Pa -> hPa */
    }

    d->temperature_c = (float)temp;
    d->pressure_hpa = (float)press;
    d->has_humidity = s_has_hum;
    d->humidity_percent = 0.0f;

    if (s_has_hum)
    {
        double h = t_fine - 76800.0;
        h = ((double)adc_H - ((double)cal.H4 * 64.0 + (double)cal.H5 / 16384.0 * h)) *
            ((double)cal.H2 / 65536.0 *
             (1.0 + (double)cal.H6 / 67108864.0 * h * (1.0 + (double)cal.H3 / 67108864.0 * h)));
        h = h * (1.0 - (double)cal.H1 * h / 524288.0);
        if (h > 100.0) h = 100.0;
        if (h < 0.0) h = 0.0;
        d->humidity_percent = (float)h;
    }

    return true;

fail:
    s_ready = false;   /* on refera l'init la prochaine fois */
    return false;
}
