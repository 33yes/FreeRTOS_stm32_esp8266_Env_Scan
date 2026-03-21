#ifndef __HUMIDIFIER_H
#define __HUMIDIFIER_H

#include "stm32f10x.h"

// 加湿器 - BIN1 (PA6)，BIN2 (PA7)
#define HUMI_GPIO             GPIOA
#define HUMI_GPIO_CLK         RCC_APB2Periph_GPIOA
#define HUMI_BIN1_PIN         GPIO_Pin_6
#define HUMI_BIN2_PIN         GPIO_Pin_7

/* ==================== 加湿器状态定义 ==================== */
typedef enum {
    HUMIDIFIER_OFF = 0,  /*!< 关闭 */
    HUMIDIFIER_ON  = 1   /*!< 开启 */
} Humidifier_State_e;

void Humidifier_Init(void);
void Humidifier_Control(Humidifier_State_e State);
Humidifier_State_e Humidifier_GetState(void);

#endif
