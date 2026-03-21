#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

// 定义结构体方便一次性获取温湿度
typedef struct {
    uint8_t temp;
    uint8_t hum;
} DHT11_Data_TypeDef;

// DHT11传感器: 温湿度检测
#define DHT11_GPIO_CLK        RCC_APB2Periph_GPIOB
#define DHT11_GPIO            GPIOB
#define DHT11_PIN             GPIO_Pin_12

/* 读取温湿度（对外推荐接口，内部包含轻量滤波，返回更稳定值） */
uint8_t DHT11_Read_Data(DHT11_Data_TypeDef *DHT11_Data);
/* 底层模式切换接口：通常仅驱动内部使用 */
void DHT11_Mode_Out(void);
void DHT11_Mode_In(void);
void DHT11_Init(void);

#endif
