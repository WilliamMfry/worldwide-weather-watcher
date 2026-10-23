#include "sd_logger.h"
#include "board_pins.h"
#include <string.h>

extern SPI_HandleTypeDef SD_SPI_HANDLE;

#define ACMD(n)  (0x80U | (n))

static SDLoggerCardType   s_type = SDLOGGER_CARD_UNKNOWN;
static SDLoggerInitStatus s_status = SDLOGGER_INIT_ERR_CMD0;
static bool    s_ready = false;
static uint8_t s_ff[512];

static uint8_t xfer(uint8_t tx)
{
    uint8_t rx = 0xFF;
    (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &tx, &rx, 1, 100);
    return rx;
}

static void flush_rx(void)
{
    (void)SD_SPI_HANDLE.Instance->DR;
    (void)SD_SPI_HANDLE.Instance->SR;
}

static void cs_low(void)  { HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET); }
static void cs_high(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    (void)xfer(0xFF);   /* un octet en plus sinon la carte bloque la ligne */
}

static void set_speed(uint32_t prescaler)
{
    SD_SPI_HANDLE.Init.BaudRatePrescaler = prescaler;
    HAL_SPI_Init(&SD_SPI_HANDLE);
}

static bool wait_ready(uint32_t timeout_ms)
{
    uint32_t t = HAL_GetTick();
    do
    {
        if (xfer(0xFF) == 0xFF) return true;
    } while ((HAL_GetTick() - t) < timeout_ms);
    return false;
}

/* envoie une commande a la carte et renvoie sa reponse (penser a remettre CS a 1 apres) */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t r;

    if (cmd & 0x80U)
    {
        cmd &= 0x7FU;
        r = send_cmd(55, 0);
        if (r > 1U) return r;
    }

    cs_high();
    cs_low();
    if (!wait_ready(500U)) return 0xFF;

    uint8_t crc = 0x01;
    if (cmd == 0) crc = 0x95;
    if (cmd == 8) crc = 0x87;

    (void)xfer((uint8_t)(0x40U | cmd));
    (void)xfer((uint8_t)(arg >> 24));
    (void)xfer((uint8_t)(arg >> 16));
    (void)xfer((uint8_t)(arg >> 8));
    (void)xfer((uint8_t)arg);
    (void)xfer(crc);

    r = 0xFF;
    for (int i = 0; i < 10; i++)
    {
        r = xfer(0xFF);
        if ((r & 0x80U) == 0U) break;
    }
    return r;
}

bool SDLogger_Init(void)
{
    uint8_t r = 0xFF;
    uint8_t ocr[4];
    uint32_t t;

    s_ready = false;
    s_type = SDLOGGER_CARD_UNKNOWN;
    memset(s_ff, 0xFF, sizeof(s_ff));

    set_speed(SPI_BAUDRATEPRESCALER_256);   /* on va doucement pendant l'init */
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
    for (int i = 0; i < 10; i++) (void)xfer(0xFF);   /* quelques coups d'horloge pour reveiller la carte */

    for (int i = 0; i < 20; i++)
    {
        r = send_cmd(0, 0);
        cs_high();
        if (r == 0x01U) break;
        HAL_Delay(5);
    }
    if (r != 0x01U) { s_status = SDLOGGER_INIT_ERR_CMD0; return false; }

    r = send_cmd(8, 0x000001AAUL);
    if (r == 0x01U)
    {
        /* cartes recentes */
        for (int i = 0; i < 4; i++) ocr[i] = xfer(0xFF);
        cs_high();
        if (ocr[2] != 0x01U || ocr[3] != 0xAAU) { s_status = SDLOGGER_INIT_ERR_CMD8; return false; }

        t = HAL_GetTick();
        do { r = send_cmd(ACMD(41), 0x40000000UL); } while (r != 0U && (HAL_GetTick() - t) < 1000U);
        cs_high();
        if (r != 0U) { s_status = SDLOGGER_INIT_ERR_ACMD41; return false; }

        r = send_cmd(58, 0);
        if (r != 0U) { cs_high(); s_status = SDLOGGER_INIT_ERR_CMD58; return false; }
        for (int i = 0; i < 4; i++) ocr[i] = xfer(0xFF);
        cs_high();

        s_type = (ocr[0] & 0x40U) ? SDLOGGER_CARD_SDHC : SDLOGGER_CARD_SDSC;
    }
    else
    {
        /* vieilles cartes */
        cs_high();
        bool ok = false;

        r = send_cmd(ACMD(41), 0);
        if (r <= 1U)
        {
            t = HAL_GetTick();
            while (r != 0U && (HAL_GetTick() - t) < 1000U) r = send_cmd(ACMD(41), 0);
            ok = (r == 0U);
        }
        else
        {
            t = HAL_GetTick();
            do { r = send_cmd(1, 0); } while (r != 0U && (HAL_GetTick() - t) < 1000U);
            ok = (r == 0U);
        }
        cs_high();
        if (!ok) { s_status = SDLOGGER_INIT_ERR_ACMD41; return false; }
        s_type = SDLOGGER_CARD_SDSC;
    }

    if (s_type == SDLOGGER_CARD_SDSC)
    {
        r = send_cmd(16, 512);
        cs_high();
        if (r != 0U) { s_status = SDLOGGER_INIT_ERR_CMD16; return false; }
    }

    set_speed(SPI_BAUDRATEPRESCALER_16);    /* on accelere, mais pas trop a cause des fils */
    s_ready = true;
    s_status = SDLOGGER_INIT_OK;
    return true;
}

bool SDLogger_ReadBlock(uint8_t *buf, uint32_t sector)
{
    if (!s_ready) return false;

    uint32_t addr = (s_type == SDLOGGER_CARD_SDHC) ? sector : (sector * 512UL);

    if (send_cmd(17, addr) != 0U) { cs_high(); return false; }

    uint8_t tok;
    uint32_t t = HAL_GetTick();
    do { tok = xfer(0xFF); } while (tok == 0xFFU && (HAL_GetTick() - t) < 200U);
    if (tok != 0xFEU) { cs_high(); return false; }

    bool ok = (HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, s_ff, buf, 512, 1000) == HAL_OK);
    (void)xfer(0xFF);   /* on s'en sert pas */
    (void)xfer(0xFF);
    cs_high();
    return ok;
}

bool SDLogger_WriteBlock(const uint8_t *buf, uint32_t sector)
{
    if (!s_ready) return false;

    uint32_t addr = (s_type == SDLOGGER_CARD_SDHC) ? sector : (sector * 512UL);

    if (send_cmd(24, addr) != 0U) { cs_high(); return false; }

    (void)xfer(0xFF);
    (void)xfer(0xFE);   /* debut des donnees */
    bool ok = (HAL_SPI_Transmit(&SD_SPI_HANDLE, (uint8_t *)buf, 512, 1000) == HAL_OK);
    flush_rx();
    (void)xfer(0xFF);   /* CRC bidon */
    (void)xfer(0xFF);

    uint8_t resp = xfer(0xFF);
    if ((resp & 0x1FU) != 0x05U) ok = false;
    if (!wait_ready(500U)) ok = false;

    cs_high();
    return ok;
}

bool SDLogger_Sync(void)
{
    if (!s_ready) return false;
    cs_low();
    bool ok = wait_ready(500U);
    cs_high();
    return ok;
}

SDLoggerCardType SDLogger_GetCardType(void)       { return s_type; }
SDLoggerInitStatus SDLogger_GetLastInitStatus(void) { return s_status; }
