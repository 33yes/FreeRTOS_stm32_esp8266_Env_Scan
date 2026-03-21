#ifndef __HC_SR501_H
#define __HC_SR501_H

#include "stm32f10x.h"

// HC-SR501: 人体红外感应 (数字输出)
#define PIR_GPIO_CLK          RCC_APB2Periph_GPIOA
#define PIR_PORT              GPIOA
#define PIR_PIN               GPIO_Pin_11  /* PA11 */
#define PIR_EXTI_LINE         EXTI_Line11

typedef enum
{
	PIR_NO_MOTION = 0,
	PIR_MOTION = 1
} PIR_State_t;

void HC_SR501_Init(void);
PIR_State_t HC_SR501_ReadRaw(void);
PIR_State_t HC_SR501_ReadStable(uint8_t sampleCount);

#endif 
