#ifndef __TASK_MANAGER_H
#define __TASK_MANAGER_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t g_task_net_mqtt_handle;
extern TaskHandle_t g_task_sensor_handle;
extern TaskHandle_t g_task_reaction_handle;
extern TaskHandle_t g_task_oled_handle;
extern TaskHandle_t g_task_status_handle;

/* 创建应用层所有任务 */
void TaskManager_CreateTasks(void);
/* 创建任务并启动调度器 */
void TaskManager_StartScheduler(void);

#endif
