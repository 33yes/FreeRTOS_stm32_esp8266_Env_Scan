#ifndef __LED_H
#define __LED_H
#include "stm32f10x.h"                  // Device header


// LED??????: PB0 (GPIO?????PWM)
#define LED_GPIO_CLK          RCC_APB2Periph_GPIOB
#define LED_GPIO              GPIOB
#define LED_PIN               GPIO_Pin_0
#define LED_TIM_CLK           RCC_APB1Periph_TIM3
#define LED_TIM               TIM3

void LED_Init(void);
void LED_SetBrightness(uint8_t Mode);
uint8_t LED_GetBrightness(void);


#endif
