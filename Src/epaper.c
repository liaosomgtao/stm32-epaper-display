/**
  ******************************************************************************
  * @file    epaper.c
  * @brief   水墨屏（SSD1680，296x128 黑白）SPI 驱动
  *
  *   - SPI 命令/数据收发、复位、忙等待
  *   - 全局刷新 / 局部刷新接口
  *   - 帧缓冲绘制（像素、矩形、7 段数码管数字时钟、预设图案）
  ******************************************************************************
  */
#include "epaper.h"
#include <string.h>

/* SPI 句柄由 main.c 定义 */
extern SPI_HandleTypeDef hspi1;

/* 全局帧缓冲 */
uint8_t EPD_Buffer[EPD_BUF_SIZE];

/* ---------- 底层引脚与 SPI 操作 ---------- */

#define CS_LOW()   HAL_GPIO_WritePin(EPD_CS_GPIO,  EPD_CS_PIN,  GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(EPD_CS_GPIO,  EPD_CS_PIN,  GPIO_PIN_SET)
#define DC_LOW()   HAL_GPIO_WritePin(EPD_DC_GPIO,  EPD_DC_PIN,  GPIO_PIN_RESET)
#define DC_HIGH()  HAL_GPIO_WritePin(EPD_DC_GPIO,  EPD_DC_PIN,  GPIO_PIN_SET)

static void EPD_WriteByte(uint8_t data)
{
    HAL_SPI_Transmit(&hspi1, &data, 1, 100);
}

static void EPD_SendCommand(uint8_t cmd)
{
    DC_LOW();
    CS_LOW();
    EPD_WriteByte(cmd);
    CS_HIGH();
}

static void EPD_SendData(uint8_t data)
{
    DC_HIGH();
    CS_LOW();
    EPD_WriteByte(data);
    CS_HIGH();
}

static void EPD_Reset(void)
{
    HAL_GPIO_WritePin(EPD_RST_GPIO, EPD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(EPD_RST_GPIO, EPD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
}

/* 忙等待：BUSY 高电平表示刷新中 */
static void EPD_WaitBusy(void)
{
    uint32_t timeout = 100000U;
    while (HAL_GPIO_ReadPin(EPD_BUSY_GPIO, EPD_BUSY_PIN) == GPIO_PIN_SET) {
        if (--timeout == 0U) {
            break;
        }
    }
}

static void EPD_SetWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
    EPD_SendCommand(0x44);            /* SET RAM X ADDRESS START/END */
    EPD_SendData((xs >> 3) & 0xFF);
    EPD_SendData((xs >> 3) >> 8);
    EPD_SendData((xe >> 3) & 0xFF);
    EPD_SendData((xe >> 3) >> 8);

    EPD_SendCommand(0x45);            /* SET RAM Y ADDRESS START/END */
    EPD_SendData(ys & 0xFF);
    EPD_SendData((ys >> 8) & 0xFF);
    EPD_SendData(ye & 0xFF);
    EPD_SendData((ye >> 8) & 0xFF);
}

static void EPD_SetCursor(uint16_t xs, uint16_t ys)
{
    EPD_SendCommand(0x4E);            /* SET RAM X ADDRESS COUNTER */
    EPD_SendData((xs >> 3) & 0xFF);
    EPD_SendData((xs >> 3) >> 8);

    EPD_SendCommand(0x4F);            /* SET RAM Y ADDRESS COUNTER */
    EPD_SendData(ys & 0xFF);
    EPD_SendData((ys >> 8) & 0xFF);
}

/* ---------- 初始化 ---------- */

void EPD_Init(void)
{
    EPD_Reset();
    EPD_WaitBusy();

    EPD_SendCommand(0x12);            /* SWRESET 软件复位 */
    EPD_WaitBusy();

    EPD_SendCommand(0x01);            /* Driver Output Control */
    EPD_SendData(0x00);
    EPD_SendData((EPD_HEIGHT - 1) & 0xFF);   /* 127 */
    EPD_SendData(0x00);

    EPD_SendCommand(0x3C);            /* Border Waveform */
    EPD_SendData(0x05);

    EPD_SendCommand(0x18);            /* Temperature Sensor */
    EPD_SendData(0x80);               /* 使用内部温度传感器 */

    EPD_SendCommand(0x11);            /* Data Entry Mode：X/Y 递增 */
    EPD_SendData(0x03);

    EPD_SetWindow(0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1);
    EPD_SetCursor(0, 0);

    EPD_SendCommand(0x21);            /* Display Update Control 1（黑白） */
    EPD_SendData(0x00);

    EPD_SendCommand(0x2C);            /* VCOM 电压（可按面板微调） */
    EPD_SendData(0x26);
}

/* ---------- 刷新接口 ---------- */

void EPD_Display(void)
{
    uint32_t i;

    EPD_SendCommand(0x24);            /* WRITE RAM（黑白数据） */
    DC_HIGH();
    CS_LOW();
    for (i = 0; i < EPD_BUF_SIZE; i++) {
        EPD_WriteByte(EPD_Buffer[i]);
    }
    CS_HIGH();

    EPD_SendCommand(0x22);            /* Display Update Control 2 */
    EPD_SendData(0xC7);               /* 全局刷新 */
    EPD_SendCommand(0x20);            /* Master Activation */
    EPD_WaitBusy();
}

void EPD_DisplayPartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t x_byte = x >> 3;         /* 要求 x 为 8 的倍数 */
    uint16_t w_byte = w >> 3;
    uint16_t row, col;

    EPD_SetWindow(x, y, x + w - 1, y + h - 1);
    EPD_SetCursor(x, y);

    EPD_SendCommand(0x24);
    DC_HIGH();
    CS_LOW();
    for (row = 0; row < h; row++) {
        for (col = 0; col < w_byte; col++) {
            EPD_WriteByte(EPD_Buffer[(y + row) * EPD_WIDTH_BYTES + x_byte + col]);
        }
    }
    CS_HIGH();

    EPD_SendCommand(0x22);
    EPD_SendData(0x0F);               /* 局部刷新 */
    EPD_SendCommand(0x20);
    EPD_WaitBusy();
}

/* ---------- 绘制接口 ---------- */

void EPD_Clear(uint8_t color)
{
    memset(EPD_Buffer, color ? 0xFF : 0x00, EPD_BUF_SIZE);
}

void EPD_SetPixel(uint16_t x, uint16_t y, uint8_t color)
{
    uint32_t idx;
    uint8_t  mask;

    if (x >= EPD_WIDTH || y >= EPD_HEIGHT) {
        return;
    }
    idx  = (uint32_t)y * EPD_WIDTH_BYTES + (x >> 3);
    mask = 0x80U >> (x & 0x07);
    if (color) {
        EPD_Buffer[idx] |= mask;
    } else {
        EPD_Buffer[idx] &= (uint8_t)~mask;
    }
}

void EPD_DrawFilledRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color)
{
    uint16_t i, j;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            EPD_SetPixel(x + j, y + i, color);
        }
    }
}

/* ---------- 7 段数码管数字 ---------- */

/* 段位：bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g */
static const uint8_t SEG_DIGIT[10] = {
    0x3F, /* 0: a b c d e f */
    0x06, /* 1: b c */
    0x5B, /* 2: a b g e d */
    0x4F, /* 3: a b g c d */
    0x66, /* 4: f g b c */
    0x6D, /* 5: a f g c d */
    0x7D, /* 6: a f g e c d */
    0x07, /* 7: a b c */
    0x7F, /* 8: 全亮 */
    0x6F  /* 9: a b c d f g */
};

static void EPD_Draw7SegDigit(uint16_t x, uint16_t y, uint8_t digit,
                              uint16_t w, uint16_t h, uint16_t t, uint8_t color)
{
    uint8_t seg = SEG_DIGIT[digit % 10];
    uint16_t hh = h / 2;

    if (seg & 0x01) EPD_DrawFilledRect(x + t,       y,             w - 2 * t, t,         color); /* a */
    if (seg & 0x02) EPD_DrawFilledRect(x + w - t,   y + t,         t,          hh - t,     color); /* b */
    if (seg & 0x04) EPD_DrawFilledRect(x + w - t,   y + hh,        t,          hh - t,     color); /* c */
    if (seg & 0x08) EPD_DrawFilledRect(x + t,       y + h - t,     w - 2 * t, t,         color); /* d */
    if (seg & 0x10) EPD_DrawFilledRect(x,           y + hh,        t,          hh - t,     color); /* e */
    if (seg & 0x20) EPD_DrawFilledRect(x,           y + t,         t,          hh - t,     color); /* f */
    if (seg & 0x40) EPD_DrawFilledRect(x + t,       y + hh - t/2,  w - 2 * t, t,         color); /* g */
}

static void EPD_DrawColon(uint16_t x, uint16_t y, uint16_t size, uint8_t color)
{
    EPD_DrawFilledRect(x, y + size,            size, size, color);
    EPD_DrawFilledRect(x, y + size * 3,        size, size, color);
}

/* 居中绘制 HH:MM:SS 数字时钟 */
void EPD_DrawClock(uint8_t hh, uint8_t mm, uint8_t ss)
{
    uint16_t dw = 30, dh = 56, th = 6, cw = 12, gap = 4;
    uint16_t total = dw * 6 + cw * 2 + gap * 7;
    uint16_t x = (EPD_WIDTH - total) / 2;
    uint16_t y = (EPD_HEIGHT - dh) / 2;

    uint8_t digits[6];
    digits[0] = hh / 10; digits[1] = hh % 10;
    digits[2] = mm / 10; digits[3] = mm % 10;
    digits[4] = ss / 10; digits[5] = ss % 10;

    EPD_Clear(0);

    EPD_Draw7SegDigit(x, y, digits[0], dw, dh, th, 1); x += dw + gap;
    EPD_Draw7SegDigit(x, y, digits[1], dw, dh, th, 1); x += dw + gap;
    EPD_DrawColon(x, y + th, cw, 1);                    x += cw + gap;
    EPD_Draw7SegDigit(x, y, digits[2], dw, dh, th, 1); x += dw + gap;
    EPD_Draw7SegDigit(x, y, digits[3], dw, dh, th, 1); x += dw + gap;
    EPD_DrawColon(x, y + th, cw, 1);                    x += cw + gap;
    EPD_Draw7SegDigit(x, y, digits[4], dw, dh, th, 1); x += dw + gap;
    EPD_Draw7SegDigit(x, y, digits[5], dw, dh, th, 1);
}

/* 预设图案：边框 + 中心方块 + 四角装饰 */
void EPD_DrawPattern(void)
{
    EPD_Clear(0);

    EPD_DrawFilledRect(4, 4, EPD_WIDTH - 8, 4, 1);                /* 上边框 */
    EPD_DrawFilledRect(4, EPD_HEIGHT - 8, EPD_WIDTH - 8, 4, 1);   /* 下边框 */
    EPD_DrawFilledRect(4, 4, 4, EPD_HEIGHT - 8, 1);               /* 左边框 */
    EPD_DrawFilledRect(EPD_WIDTH - 8, 4, 4, EPD_HEIGHT - 8, 1);   /* 右边框 */

    EPD_DrawFilledRect(EPD_WIDTH / 2 - 24, EPD_HEIGHT / 2 - 24, 48, 48, 1); /* 中心方块 */

    EPD_DrawFilledRect(24, 24, 20, 20, 1);                        /* 四角装饰 */
    EPD_DrawFilledRect(EPD_WIDTH - 44, 24, 20, 20, 1);
    EPD_DrawFilledRect(24, EPD_HEIGHT - 44, 20, 20, 1);
    EPD_DrawFilledRect(EPD_WIDTH - 44, EPD_HEIGHT - 44, 20, 20, 1);
}
