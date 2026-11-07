/**
  ******************************************************************************
  * @file    main.c
  * @brief   水墨屏显示主程序（STM32F103 + FreeRTOS）
  *
  *   两个任务：
  *     DisplayTask —— 显示刷新任务：数字时钟走时 + 模式切换全局刷新
  *     ButtonTask —— 按键响应任务：去抖、切换模式、队列通知
  *
  *   硬件：SPI1(PA5/PA7) 接水墨屏；PB0=CS PB1=DC PB2=RST PB3=BUSY；PA0=按键
  ******************************************************************************
  */
#include "stm32f1xx_hal.h"
#include "epaper.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* 外设句柄 */
SPI_HandleTypeDef hspi1;

/* 按键 */
#define BTN_GPIO   GPIOA
#define BTN_PIN    GPIO_PIN_0

/* 显示模式 */
#define MODE_CLOCK    0U
#define MODE_PATTERN  1U

/* 任务间通信：按键任务 -> 显示任务，传递新显示模式 */
QueueHandle_t modeQueue;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void Error_Handler(void);

static void DisplayTask(void *arg);
static void ButtonTask(void *arg);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();

    EPD_Init();
    EPD_DrawClock(0, 0, 0);
    EPD_Display();

    modeQueue = xQueueCreate(4, sizeof(uint8_t));
    if (modeQueue == NULL) {
        Error_Handler();
    }

    xTaskCreate(DisplayTask, "display", 512, NULL, 2, NULL);
    xTaskCreate(ButtonTask, "button",  256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) { }
}

/* 显示刷新任务：1s 走时，收到消息则全局刷新切换模式 */
static void DisplayTask(void *arg)
{
    (void)arg;
    uint8_t mode = MODE_CLOCK;
    uint8_t newMode;
    uint32_t seconds = 0;
    uint8_t hh, mm, ss;

    for (;;) {
        /* 阻塞等待按键消息，超时 1s 用于走时 */
        if (xQueueReceive(modeQueue, &newMode, pdMS_TO_TICKS(1000)) == pdTRUE) {
            mode = newMode;
            if (mode == MODE_CLOCK) {
                EPD_DrawClock(0, 0, 0);      /* 重新进入时钟，先全刷一帧 */
            } else {
                EPD_DrawPattern();
            }
            EPD_Display();                   /* 全局刷新 */
            seconds = 0;
            continue;
        }

        if (mode == MODE_CLOCK) {
            hh = (uint8_t)((seconds / 3600) % 24);
            mm = (uint8_t)((seconds / 60) % 60);
            ss = (uint8_t)(seconds % 60);
            EPD_DrawClock(hh, mm, ss);
            /* 走时每秒局部刷新时钟区域，减少刷新时间 */
            EPD_DisplayPartial(0, 0, EPD_WIDTH, EPD_HEIGHT);
            seconds++;
        }
    }
}

/* 按键响应任务：去抖、切换模式并通过队列通知 */
static void ButtonTask(void *arg)
{
    (void)arg;
    uint8_t mode = MODE_CLOCK;

    for (;;) {
        if (HAL_GPIO_ReadPin(BTN_GPIO, BTN_PIN) == GPIO_PIN_RESET) {
            vTaskDelay(pdMS_TO_TICKS(20));             /* 去抖 */
            if (HAL_GPIO_ReadPin(BTN_GPIO, BTN_PIN) == GPIO_PIN_RESET) {
                mode = (mode == MODE_CLOCK) ? MODE_PATTERN : MODE_CLOCK;
                xQueueSend(modeQueue, &mode, 0);
                /* 等待按键释放，避免一次按下多次触发 */
                while (HAL_GPIO_ReadPin(BTN_GPIO, BTN_PIN) == GPIO_PIN_RESET) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ==================== HAL 初始化（可由 CubeMX 生成替换） ==================== */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSE 8MHz -> PLL x9 -> 72MHz */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 72/8=9MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* SPI1：PA5(SCK)/PA7(MOSI)，复用推挽 */
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 水墨屏控制引脚：PB0=CS PB1=DC PB2=RST（输出），PB3=BUSY（输入） */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 按键：PA0 输入上拉（低电平有效） */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_SPI1_CLK_ENABLE();
    }
}

static void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
