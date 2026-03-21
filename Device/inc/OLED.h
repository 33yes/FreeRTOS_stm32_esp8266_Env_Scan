#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"                  // Device header

// 软件I2C: OLED显示屏控制
#define OLED_GPIO_CLK         RCC_APB2Periph_GPIOB
#define OLED_GPIO             GPIOB
#define OLED_SCL_PIN          GPIO_Pin_8
#define OLED_SDA_PIN          GPIO_Pin_9

void OLED_Clear(void);
void OLED_Init(void);

void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

// 以下函数仅在OLED.c中定义为static，头文件不声明：
// - OLED_I2C_Start
// - OLED_I2C_Stop
// - OLED_I2C_SendByte
// - OLED_WriteCommand
// - OLED_WriteData
// - OLED_SetCursor
// - OLED_Pow

#endif
