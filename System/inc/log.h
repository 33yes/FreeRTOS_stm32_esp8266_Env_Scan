#ifndef __LOG_H
#define __LOG_H

#include "stm32f10x.h"
#include <stdio.h>

#define LOG_USART               USART1
#define LOG_USART_CLK           RCC_APB2Periph_USART1
#define LOG_GPIO                GPIOA
#define LOG_GPIO_CLK            RCC_APB2Periph_GPIOA
#define LOG_TX_PIN              GPIO_Pin_9
#define LOG_RX_PIN              GPIO_Pin_10

void LOG_Init(uint32_t baudrate);
void LOG_WriteChar(char ch);
void LOG_WriteString(const char *str);

#endif
