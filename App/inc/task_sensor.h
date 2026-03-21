#ifndef __TASK_SENSOR_H
#define __TASK_SENSOR_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* 传感器任务输出的数据结构（供队列与其他任务读取） */
typedef struct
{
	int16_t temperature;   /* 温度（℃） */
	int16_t humidity;      /* 湿度（%RH） */
	uint16_t light;        /* 光照（lux） */
	uint8_t motion;        /* 人体检测（0:无人 1:有人） */
	TickType_t tick;       /* 采样时刻 */
} SensorData_t;

/* 队列长度为1，始终保存最新采样值 */
extern QueueHandle_t g_sensor_queue;

/* 初始化传感器底层并创建数据队列 */
void TaskSensor_Init(void);
/* 传感器任务：周期采集 -> 投递队列 -> 网络在线时上报 */
void Task_Sensor(void *pvParameters);
/* 获取最近一次采样快照（非阻塞） */
BaseType_t TaskSensor_GetLatest(SensorData_t *outData);

#endif
