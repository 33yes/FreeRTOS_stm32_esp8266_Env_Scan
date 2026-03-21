#ifndef __TASK_NET_H
#define __TASK_NET_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "mqtt.h"

typedef enum
{
	NET_MQTT_STATE_IDLE = 0,
	NET_MQTT_STATE_WIFI_CONNECTED,
	NET_MQTT_STATE_MQTT_WAIT_CONNACK,
	NET_MQTT_STATE_RUNNING
} NetMqttState_t;

/* 网络任务运行时状态快照 */
typedef struct
{
	NetMqttState_t state;
	uint8_t mqtt_connected;
	uint8_t wifi_connected;
	uint32_t reconnect_count;
	TickType_t last_ping_tick;
	TickType_t last_pong_tick;
	TickType_t last_report_tick;
} NetMqttRuntime_t;

extern NetMqttRuntime_t g_net_mqtt_runtime;

/* 网络主任务：负责连接、收发、心跳和下行处理 */
void Task_NetMqtt(void *pvParameters);
/* 上报网络状态+阈值+设备状态到云端 */
BaseType_t TaskNetMqtt_PublishStatusAndThreshold(void);
/* 查询MQTT连接状态 */
BaseType_t TaskNetMqtt_IsConnected(void);
/* 获取网络运行状态指针 */
const NetMqttRuntime_t *TaskNetMqtt_GetRuntime(void);

#endif
