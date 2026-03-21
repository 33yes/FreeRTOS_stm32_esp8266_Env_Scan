#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

/* ==================== 电机速度档位定义 ==================== */
typedef enum {
    MOTOR_STOP   = 0,    /*!< 停止 */
    MOTOR_LEVEL1 = 50,   /*!< 一档（50% PWM）*/
    MOTOR_LEVEL2 = 100   /*!< 二档（100% PWM）*/
} Motor_Speed_e;

// 电机通道A PWMA (TB6612 PWMA): TIM2_CH1 (PA0)
#define MOTOR_PWMA_TIM_CLK    RCC_APB1Periph_TIM2
#define MOTOR_PWMA_TIM        TIM2
#define MOTOR_PWMA_GPIO_CLK   RCC_APB2Periph_GPIOA
#define MOTOR_PWMA_GPIO       GPIOA
#define MOTOR_PWMA_PIN        GPIO_Pin_0

// 电机方向控制 - AIN1 (PA4)，AIN2 (PA5)
#define MOTOR_GPIO_CLK   RCC_APB2Periph_GPIOA
#define MOTOR_GPIO       GPIOA
#define MOTOR_AIN1_PIN        GPIO_Pin_4
#define MOTOR_AIN2_PIN        GPIO_Pin_5

void Motor_Init(void);
void Motor_SetSpeed(Motor_Speed_e Speed);
void Motor_Stop(void);
Motor_Speed_e Motor_GetSpeed(void);


#endif
