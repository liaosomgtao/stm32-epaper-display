/**
  ******************************************************************************
  * @file    epaper.h
  * @brief   水墨屏（SSD1680，296x128）驱动头文件
  ******************************************************************************
  */
#ifndef __EPAPER_H
#define __EPAPER_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* 屏幕尺寸（2.9" 黑白，SSD1680） */
#define EPD_WIDTH        296U
#define EPD_HEIGHT       128U
#define EPD_WIDTH_BYTES  37U                       /* 296 / 8 */
#define EPD_BUF_SIZE     (EPD_WIDTH_BYTES * EPD_HEIGHT)  /* 4736 字节 */

/* 引脚定义（可改） */
#define EPD_CS_GPIO      GPIOB
#define EPD_CS_PIN       GPIO_PIN_0
#define EPD_DC_GPIO      GPIOB
#define EPD_DC_PIN       GPIO_PIN_1
#define EPD_RST_GPIO     GPIOB
#define EPD_RST_PIN      GPIO_PIN_2
#define EPD_BUSY_GPIO    GPIOB
#define EPD_BUSY_PIN     GPIO_PIN_3

/* 全局帧缓冲（1 bit/像素，MSB 在前） */
extern uint8_t EPD_Buffer[EPD_BUF_SIZE];

void EPD_Init(void);
void EPD_Clear(uint8_t color);                              /* 0=白 1=黑 */
void EPD_SetPixel(uint16_t x, uint16_t y, uint8_t color);
void EPD_DrawFilledRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color);

void EPD_Display(void);                                     /* 全局刷新 */
void EPD_DisplayPartial(uint16_t x, uint16_t y, uint16_t w, uint16_t h);  /* 局部刷新 */

void EPD_DrawClock(uint8_t hh, uint8_t mm, uint8_t ss);     /* 数字时钟 */
void EPD_DrawPattern(void);                                 /* 预设图案 */

#endif /* __EPAPER_H */
