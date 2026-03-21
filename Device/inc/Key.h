#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

// 按键输入: 菜单导航/功能控制
#define KEY_GPIO              GPIOB
#define KEY1_PIN              GPIO_Pin_13  /* PB13: 菜单光标上移 */
#define KEY2_PIN              GPIO_Pin_14  /* PB14: 菜单确认 */
#define KEY3_PIN              GPIO_Pin_15  /* PB15: 菜单光标下移 */

// 键码返回值定义
#define KEY_NONE        0
#define KEY_1           1   /* 光标上移 */
#define KEY_2           2   /* 确认 */
#define KEY_3           3   /* 光标下移 */

void Key_Init(void);
uint8_t Key_GetNum(void);

#endif
