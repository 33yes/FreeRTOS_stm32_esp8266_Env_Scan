#include "stm32f10x.h"                  // Device header
#include "LED.h"  

/* 当前LED档位状态（0:关 1:一档 2:二档） */
static uint8_t s_led_mode = 0;

/**
  * @brief  LED 初始化 (使用 TIM3 通道 3, PB0)
  */
void LED_Init(void)
{
    /* 1. 开启时钟 */
    RCC_APB1PeriphClockCmd(LED_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(LED_GPIO_CLK, ENABLE);
    
    /* 2. GPIO 配置为复用推挽输出 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; 
    GPIO_InitStructure.GPIO_Pin = LED_PIN;        // PB0
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_GPIO, &GPIO_InitStructure);

    /* 3. 时基单元配置 (20kHz) */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;     // ARR: 0-100 方便对应亮度百分比
    TIM_TimeBaseInitStructure.TIM_Prescaler = 36 - 1;  // PSC: 72MHz/(36*100) = 20kHz
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(LED_TIM, &TIM_TimeBaseInitStructure);

    /* 4. 输出比较配置 (通道 3) */
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;                 // 初始亮度为 0
    TIM_OC3Init(LED_TIM, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(LED_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(LED_TIM, ENABLE);

    /* 5. 使能定时器 */
    TIM_Cmd(LED_TIM, ENABLE);
}

/**
  * @brief  设置 LED 亮度档位
  * @param  Mode: 0-关, 1-一档(30%亮度), 2-二档(100%亮度)
  */
void LED_SetBrightness(uint8_t Mode)
{
  if (Mode == 0)
  {
    TIM_SetCompare3(LED_TIM, 0);   // 关闭
    s_led_mode = 0;
  }
  else if (Mode == 1)
  {
    TIM_SetCompare3(LED_TIM, 30);  // 一档：柔和
    s_led_mode = 1;
  }
  else if (Mode == 2)
  {
    TIM_SetCompare3(LED_TIM, 99);  // 二档：全亮
    s_led_mode = 2;
  }
  else
  {
    TIM_SetCompare3(LED_TIM, 0);   // 非法参数保护
    s_led_mode = 0;
  }
}

/* 读取当前LED档位（用于OLED显示/云端状态上报） */
uint8_t LED_GetBrightness(void)
{
  return s_led_mode;
}