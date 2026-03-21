#include "task_reaction.h"

#include "task_sensor.h"
#include "mqtt.h"
#include "LED.h"

#define REACTION_TASK_PERIOD_MS    200

/* 当前执行器运行状态 */
static ReactionStatus_t g_reaction_status =
{
	CTRL_MODE_AUTO,
	CTRL_MODE_AUTO,
	CTRL_MODE_AUTO,
	0,
	MOTOR_STOP,
	HUMIDIFIER_OFF
};

/* 自动控制：根据“光照阈值+人体检测”调整灯光 */
static uint8_t Reaction_AutoLightLevel(uint16_t light, uint8_t motion, uint16_t lightThreshold)
{
	uint32_t darkness_ratio;

	if ((motion == 0U) || (lightThreshold == 0U) || (light >= lightThreshold))
	{
		return 0U;
	}

	darkness_ratio = ((uint32_t)(lightThreshold - light) * 100U) / lightThreshold;
	if (darkness_ratio >= 60U)
	{
		return 2U;
	}

	return 1U;
}

/* 自动控制：根据温度阈值调整电机档位 */
static Motor_Speed_e Reaction_AutoMotorLevel(int16_t temperature, int16_t tempThreshold)
{
	if (temperature >= (int16_t)(tempThreshold + 5))
	{
		return MOTOR_LEVEL2;
	}

	if (temperature >= tempThreshold)
	{
		return MOTOR_LEVEL1;
	}

	return MOTOR_STOP;
}

/* 自动控制：根据湿度阈值调整加湿器开关（湿度偏低时开启） */
static Humidifier_State_e Reaction_AutoHumidifier(int16_t humidity, int16_t humidityThreshold)
{
	if (humidity <= humidityThreshold)
	{
		return HUMIDIFIER_ON;
	}

	return HUMIDIFIER_OFF;
}

/* 按当前状态将控制量下发到底层驱动 */
static void Reaction_ApplyOutput(void)
{
	LED_SetBrightness(g_reaction_status.light_level);
	Motor_SetSpeed(g_reaction_status.motor_level);
	Humidifier_Control(g_reaction_status.humidifier_on);
}

/* 自动模式刷新：读取传感器和阈值后，更新自动控制输出 */
static void Reaction_UpdateAutoBySensor(void)
{
	SensorData_t sensor;

	if (TaskSensor_GetLatest(&sensor) != pdPASS)
	{
		return;
	}

	if (g_reaction_status.light_mode == CTRL_MODE_AUTO)
	{
		g_reaction_status.light_level = Reaction_AutoLightLevel(
			sensor.light,
			sensor.motion,
			g_mqtt_threshold.light_threshold
		);
	}

	if (g_reaction_status.motor_mode == CTRL_MODE_AUTO)
	{
		g_reaction_status.motor_level = Reaction_AutoMotorLevel(
			sensor.temperature,
			g_mqtt_threshold.temperature_threshold
		);
	}

	if (g_reaction_status.humidifier_mode == CTRL_MODE_AUTO)
	{
		g_reaction_status.humidifier_on = Reaction_AutoHumidifier(
			sensor.humidity,
			g_mqtt_threshold.humidity_threshold
		);
	}
}

/* 初始化执行器任务：底层驱动初始化并进入默认自动模式 */
void TaskReaction_Init(void)
{
	LED_Init();
	Motor_Init();
	Humidifier_Init();

	g_reaction_status.light_mode = CTRL_MODE_AUTO;
	g_reaction_status.motor_mode = CTRL_MODE_AUTO;
	g_reaction_status.humidifier_mode = CTRL_MODE_AUTO;
	g_reaction_status.light_level = 0;
	g_reaction_status.motor_level = MOTOR_STOP;
	g_reaction_status.humidifier_on = HUMIDIFIER_OFF;

	Reaction_ApplyOutput();
}

/* 执行器主任务：定期刷新自动控制并下发输出 */
void Task_Reaction(void *pvParameters)
{
	(void)pvParameters;
	TaskReaction_Init();

	for (;;)
	{
		Reaction_UpdateAutoBySensor();
		Reaction_ApplyOutput();
		vTaskDelay(pdMS_TO_TICKS(REACTION_TASK_PERIOD_MS));
	}
}

/* 灯恢复自动模式 */
void TaskReaction_SetLightAuto(void)
{
	taskENTER_CRITICAL();
	g_reaction_status.light_mode = CTRL_MODE_AUTO;
	taskEXIT_CRITICAL();
}

/* 灯切换手动模式并设置档位 */
void TaskReaction_SetLightManual(uint8_t level)
{
	taskENTER_CRITICAL();
	g_reaction_status.light_mode = CTRL_MODE_MANUAL;
	if (level > 2U)
	{
		level = 0U;
	}
	g_reaction_status.light_level = level;
	taskEXIT_CRITICAL();
}

/* 电机恢复自动模式 */
void TaskReaction_SetMotorAuto(void)
{
	taskENTER_CRITICAL();
	g_reaction_status.motor_mode = CTRL_MODE_AUTO;
	taskEXIT_CRITICAL();
}

/* 电机切换手动模式并设置档位 */
void TaskReaction_SetMotorManual(Motor_Speed_e level)
{
	taskENTER_CRITICAL();
	g_reaction_status.motor_mode = CTRL_MODE_MANUAL;
	if ((level != MOTOR_STOP) && (level != MOTOR_LEVEL1) && (level != MOTOR_LEVEL2))
	{
		level = MOTOR_STOP;
	}
	g_reaction_status.motor_level = level;
	taskEXIT_CRITICAL();
}

/* 加湿器恢复自动模式 */
void TaskReaction_SetHumidifierAuto(void)
{
	taskENTER_CRITICAL();
	g_reaction_status.humidifier_mode = CTRL_MODE_AUTO;
	taskEXIT_CRITICAL();
}

/* 加湿器切换手动模式并设置开关 */
void TaskReaction_SetHumidifierManual(Humidifier_State_e state)
{
	taskENTER_CRITICAL();
	g_reaction_status.humidifier_mode = CTRL_MODE_MANUAL;
	if (state != HUMIDIFIER_ON)
	{
		state = HUMIDIFIER_OFF;
	}
	g_reaction_status.humidifier_on = state;
	taskEXIT_CRITICAL();
}

/* 获取当前执行器状态快照 */
BaseType_t TaskReaction_GetStatus(ReactionStatus_t *outStatus)
{
	if (outStatus == NULL)
	{
		return pdFAIL;
	}

	taskENTER_CRITICAL();
	*outStatus = g_reaction_status;
	taskEXIT_CRITICAL();
	return pdPASS;
}
