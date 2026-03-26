#include "task_manager.h"

#include "task_net.h"
#include "task_sensor.h"
#include "task_reaction.h"
#include "task_oled.h"
#include "delay.h"
#include "stdio.h"

#define TASK_PRI_NET_MQTT             6
#define TASK_PRI_SENSOR               5
#define TASK_PRI_REACTION             4
#define TASK_PRI_OLED_UI              3
#define TASK_PRI_STATUS               2

#define TASK_STACK_NET_MQTT           512
#define TASK_STACK_SENSOR             320
#define TASK_STACK_REACTION           320
#define TASK_STACK_OLED_UI            512
#define TASK_STACK_STATUS             192

TaskHandle_t g_task_net_mqtt_handle = NULL;
TaskHandle_t g_task_sensor_handle = NULL;
TaskHandle_t g_task_reaction_handle = NULL;
TaskHandle_t g_task_oled_handle = NULL;
TaskHandle_t g_task_status_handle = NULL;

/* 系统状态任务：周期打印网络连接状态，便于串口调试 */
static void Task_SystemStatus(void *pvParameters)
{
	(void)pvParameters;

	for (;;)
	{
		const NetMqttRuntime_t *runtime = TaskNetMqtt_GetRuntime();
		printf("[NET] state=%d wifi=%d mqtt=%d reconnect=%lu\r\n",
			   (int)runtime->state,
			   (int)runtime->wifi_connected,
			   (int)runtime->mqtt_connected,
			   (unsigned long)runtime->reconnect_count);

		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

/* 创建应用层任务并设置优先级/栈大小 */
void TaskManager_CreateTasks(void)
{
	BaseType_t ret;

	ret = xTaskCreate(
		Task_NetMqtt,
		"task_net_mqtt",
		TASK_STACK_NET_MQTT,
		NULL,
		TASK_PRI_NET_MQTT,
		&g_task_net_mqtt_handle
	);

	if (ret != pdPASS)
	{
		printf("Create task_net_mqtt failed\r\n");
	}

	ret = xTaskCreate(
		Task_Sensor,
		"task_sensor",
		TASK_STACK_SENSOR,
		NULL,
		TASK_PRI_SENSOR,
		&g_task_sensor_handle
	);

	if (ret != pdPASS)
	{
		printf("Create task_sensor failed\r\n");
	}

	ret = xTaskCreate(
		Task_Reaction,
		"task_reaction",
		TASK_STACK_REACTION,
		NULL,
		TASK_PRI_REACTION,
		&g_task_reaction_handle
	);

	if (ret != pdPASS)
	{
		printf("Create task_reaction failed\r\n");
	}

	ret = xTaskCreate(
		Task_OledUi,
		"task_oled_ui",
		TASK_STACK_OLED_UI,
		NULL,
		TASK_PRI_OLED_UI,
		&g_task_oled_handle
	);

	if (ret != pdPASS)
	{
		printf("Create task_oled_ui failed\r\n");
	}

	ret = xTaskCreate(
		Task_SystemStatus,
		"task_status",
		TASK_STACK_STATUS,
		NULL,
		TASK_PRI_STATUS,
		&g_task_status_handle
	);

	if (ret != pdPASS)
	{
		printf("Create task_status failed\r\n");
	}
}

/* 启动调度器入口 */
void TaskManager_StartScheduler(void)
{
	TaskManager_CreateTasks();
	vTaskStartScheduler();

	while (1)
	{
		delay_ms(1000);
	}
}
