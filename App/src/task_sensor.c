#include "task_sensor.h"

#include "DHT11.h"
#include "BH1750.h"
#include "HC_SR501.h"
#include "mqtt.h"
#include "task_net.h"
#include "stdio.h"

#define SENSOR_QUEUE_LENGTH            1
#define SENSOR_SAMPLING_PERIOD_MS      1000
#define SENSOR_PUBLISH_PERIOD_MS       5000

QueueHandle_t g_sensor_queue = NULL;

static SensorData_t g_sensor_latest = {0};

/* 单次采样：读取温湿度/光照/人体检测 */
static void TaskSensor_Sample(SensorData_t *data)
{
	DHT11_Data_TypeDef dht11_data;

	if (DHT11_Read_Data(&dht11_data) == 0)
	{
		data->temperature = (int16_t)dht11_data.temp;
		data->humidity = (int16_t)dht11_data.hum;
	}

	data->light = (uint16_t)BH1750_Read_Lux();
	data->motion = (uint8_t)HC_SR501_ReadStable(5);
	data->tick = xTaskGetTickCount();
}

/* 将采样数据打包为JSON并写入 MQTT 发送缓冲区 */
static void TaskSensor_Publish(const SensorData_t *data)
{
	char payload[160];
	int payload_len;

	payload_len = snprintf(
		payload,
		sizeof(payload),
		"{\"id\":\"2\",\"version\":\"1.0\",\"params\":{\"temperature\":{\"value\":%d},\"humidity\":{\"value\":%d},\"light\":{\"value\":%u},\"motion\":{\"value\":%u}}}",
		(int)data->temperature,
		(int)data->humidity,
		(unsigned int)data->light,
		(unsigned int)data->motion
	);

	if ((payload_len <= 0) || (payload_len >= (int)sizeof(payload)))
	{
		return;
	}

	taskENTER_CRITICAL();
	MQTT_PublishQs0(DATA_TOPIC_NAME, payload, payload_len);
	taskEXIT_CRITICAL();
}

/* 初始化本任务依赖的底层驱动和队列 */
void TaskSensor_Init(void)
{
	DHT11_Init();
	BH1750_Init();
	BH1750_Init_Config();
	HC_SR501_Init();

	if (g_sensor_queue == NULL)
	{
		g_sensor_queue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorData_t));
	}
}

/* 传感器主任务：周期采样，更新队列，按周期上报云端 */
void Task_Sensor(void *pvParameters)
{
	TickType_t last_publish_tick;
	SensorData_t sample;

	(void)pvParameters;
	TaskSensor_Init();
	last_publish_tick = 0;

	for (;;)
	{
		TaskSensor_Sample(&sample);
		g_sensor_latest = sample;

		if (g_sensor_queue != NULL)
		{
			xQueueOverwrite(g_sensor_queue, &sample);
		}

		if ((TaskNetMqtt_IsConnected() == pdPASS) &&
			((xTaskGetTickCount() - last_publish_tick) >= pdMS_TO_TICKS(SENSOR_PUBLISH_PERIOD_MS)))
		{
			TaskSensor_Publish(&sample);
			last_publish_tick = xTaskGetTickCount();
		}

		vTaskDelay(pdMS_TO_TICKS(SENSOR_SAMPLING_PERIOD_MS));
	}
}

/* 获取最近一次采样值（用于 OLED/控制任务快速读取） */
BaseType_t TaskSensor_GetLatest(SensorData_t *outData)
{
	if (outData == NULL)
	{
		return pdFAIL;
	}

	*outData = g_sensor_latest;
	return pdPASS;
}
