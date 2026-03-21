#ifndef __TASK_OLED_H
#define __TASK_OLED_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

/* 初始化OLED界面与按键输入 */
void TaskOledUi_Init(void);
/* OLED+按键主任务：菜单渲染与交互处理 */
void Task_OledUi(void *pvParameters);

#endif
