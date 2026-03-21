#include "task_oled.h"

#include "OLED.h"
#include "Key.h"
#include "task_sensor.h"
#include "task_net.h"
#include "task_reaction.h"
#include "stdio.h"

#define OLED_UI_PERIOD_MS    150

typedef enum
{
	UI_PAGE_HOME = 0,
	UI_PAGE_MENU,
	UI_PAGE_MOTOR,
	UI_PAGE_LIGHT,
	UI_PAGE_HUMIDIFIER
} UiPage_t;

static UiPage_t s_ui_page = UI_PAGE_HOME;
static uint8_t s_menu_cursor = 0;
static uint8_t s_motor_cursor = 0;
static uint8_t s_light_cursor = 0;
static uint8_t s_humi_cursor = 0;

/* 将灯状态转换为短文本（用于OLED显示） */
static const char *Ui_LightStatusText(const ReactionStatus_t *status)
{
	if (status->light_mode == CTRL_MODE_AUTO)
	{
		return "AUTO";
	}

	if (status->light_level == 1U)
	{
		return "L1";
	}
	if (status->light_level == 2U)
	{
		return "L2";
	}

	return "OFF";
}

/* 将电机状态转换为短文本（用于OLED显示） */
static const char *Ui_MotorStatusText(const ReactionStatus_t *status)
{
	if (status->motor_mode == CTRL_MODE_AUTO)
	{
		return "AUTO";
	}

	if (status->motor_level == MOTOR_LEVEL1)
	{
		return "L1";
	}
	if (status->motor_level == MOTOR_LEVEL2)
	{
		return "L2";
	}

	return "OFF";
}

/* 将加湿器状态转换为短文本（用于OLED显示） */
static const char *Ui_HumidifierStatusText(const ReactionStatus_t *status)
{
	if (status->humidifier_mode == CTRL_MODE_AUTO)
	{
		return "AUTO";
	}

	if (status->humidifier_on == HUMIDIFIER_ON)
	{
		return "ON";
	}

	return "OFF";
}

/* 首页：显示关键传感器参数、网络状态和设备状态 */
static void Ui_RenderHome(void)
{
	char line[24];
	SensorData_t sensor;
	ReactionStatus_t react;
	const NetMqttRuntime_t *net;

	(void)TaskSensor_GetLatest(&sensor);
	(void)TaskReaction_GetStatus(&react);
	net = TaskNetMqtt_GetRuntime();

	OLED_Clear();
	snprintf(line, sizeof(line), "T:%d H:%d L:%u", (int)sensor.temperature, (int)sensor.humidity, (unsigned int)sensor.light);
	OLED_ShowString(1, 1, line);

	snprintf(line, sizeof(line), "NET:%s PIR:%u", (net->mqtt_connected != 0U) ? "ON" : "OFF", (unsigned int)sensor.motion);
	OLED_ShowString(2, 1, line);

	snprintf(line, sizeof(line), "L:%s M:%s H:%s", Ui_LightStatusText(&react), Ui_MotorStatusText(&react), Ui_HumidifierStatusText(&react));
	OLED_ShowString(3, 1, line);

	OLED_ShowString(4, 1, "K2->MENU");
}

/* 二级菜单：电机/灯/加湿器/返回（两行两列） */
static void Ui_RenderMenu(void)
{
	char line[24];

	OLED_Clear();
	snprintf(line, sizeof(line), "%cMOTOR %cLIGHT", (s_menu_cursor == 0U) ? '>' : ' ', (s_menu_cursor == 1U) ? '>' : ' ');
	OLED_ShowString(1, 1, line);

	snprintf(line, sizeof(line), "%cHUMID %cBACK", (s_menu_cursor == 2U) ? '>' : ' ', (s_menu_cursor == 3U) ? '>' : ' ');
	OLED_ShowString(2, 1, line);

	OLED_ShowString(3, 1, "K1:UP K3:DOWN");
	OLED_ShowString(4, 1, "K2:OK");
}

/* 电机控制页面渲染 */
static void Ui_RenderMotorPage(void)
{
	char line[24];
	ReactionStatus_t react;
	(void)TaskReaction_GetStatus(&react);

	OLED_Clear();
	snprintf(line, sizeof(line), "motor:%s", Ui_MotorStatusText(&react));
	OLED_ShowString(1, 1, line);
	snprintf(line, sizeof(line), "%cOFF %cL1", (s_motor_cursor == 0U) ? '>' : ' ', (s_motor_cursor == 1U) ? '>' : ' ');
	OLED_ShowString(2, 1, line);
	snprintf(line, sizeof(line), "%cL2  %cAUTO", (s_motor_cursor == 2U) ? '>' : ' ', (s_motor_cursor == 3U) ? '>' : ' ');
	OLED_ShowString(3, 1, line);
	snprintf(line, sizeof(line), "%cBACK", (s_motor_cursor == 4U) ? '>' : ' ');
	OLED_ShowString(4, 1, line);
}

/* 灯光控制页面渲染 */
static void Ui_RenderLightPage(void)
{
	char line[24];
	ReactionStatus_t react;
	(void)TaskReaction_GetStatus(&react);

	OLED_Clear();
	snprintf(line, sizeof(line), "light:%s", Ui_LightStatusText(&react));
	OLED_ShowString(1, 1, line);
	snprintf(line, sizeof(line), "%cOFF %cL1", (s_light_cursor == 0U) ? '>' : ' ', (s_light_cursor == 1U) ? '>' : ' ');
	OLED_ShowString(2, 1, line);
	snprintf(line, sizeof(line), "%cL2  %cAUTO", (s_light_cursor == 2U) ? '>' : ' ', (s_light_cursor == 3U) ? '>' : ' ');
	OLED_ShowString(3, 1, line);
	snprintf(line, sizeof(line), "%cBACK", (s_light_cursor == 4U) ? '>' : ' ');
	OLED_ShowString(4, 1, line);
}

/* 加湿器控制页面渲染 */
static void Ui_RenderHumidifierPage(void)
{
	char line[24];
	ReactionStatus_t react;
	(void)TaskReaction_GetStatus(&react);

	OLED_Clear();
	snprintf(line, sizeof(line), "humid:%s", Ui_HumidifierStatusText(&react));
	OLED_ShowString(1, 1, line);
	snprintf(line, sizeof(line), "%cON   %cOFF", (s_humi_cursor == 0U) ? '>' : ' ', (s_humi_cursor == 1U) ? '>' : ' ');
	OLED_ShowString(2, 1, line);
	snprintf(line, sizeof(line), "%cAUTO", (s_humi_cursor == 2U) ? '>' : ' ');
	OLED_ShowString(3, 1, line);
	snprintf(line, sizeof(line), "%cBACK", (s_humi_cursor == 3U) ? '>' : ' ');
	OLED_ShowString(4, 1, line);
}

/* 根据按键处理页面逻辑 */
static void Ui_HandleKey(uint8_t key)
{
	if (key == KEY_NONE)
	{
		return;
	}

	if (s_ui_page == UI_PAGE_HOME)
	{
		if (key == KEY_2)
		{
			s_ui_page = UI_PAGE_MENU;
			s_menu_cursor = 0;
		}
		return;
	}

	if (s_ui_page == UI_PAGE_MENU)
	{
		if (key == KEY_1)
		{
			s_menu_cursor = (s_menu_cursor == 0U) ? 3U : (uint8_t)(s_menu_cursor - 1U);
		}
		else if (key == KEY_3)
		{
			s_menu_cursor = (uint8_t)((s_menu_cursor + 1U) % 4U);
		}
		else if (key == KEY_2)
		{
			if (s_menu_cursor == 0U)
			{
				s_ui_page = UI_PAGE_MOTOR;
				s_motor_cursor = 0;
			}
			else if (s_menu_cursor == 1U)
			{
				s_ui_page = UI_PAGE_LIGHT;
				s_light_cursor = 0;
			}
			else if (s_menu_cursor == 2U)
			{
				s_ui_page = UI_PAGE_HUMIDIFIER;
				s_humi_cursor = 0;
			}
			else
			{
				s_ui_page = UI_PAGE_HOME;
			}
		}
		return;
	}

	if (s_ui_page == UI_PAGE_MOTOR)
	{
		if (key == KEY_1)
		{
			s_motor_cursor = (s_motor_cursor == 0U) ? 4U : (uint8_t)(s_motor_cursor - 1U);
		}
		else if (key == KEY_3)
		{
			s_motor_cursor = (uint8_t)((s_motor_cursor + 1U) % 5U);
		}
		else if (key == KEY_2)
		{
			if (s_motor_cursor == 0U)
			{
				TaskReaction_SetMotorManual(MOTOR_STOP);
			}
			else if (s_motor_cursor == 1U)
			{
				TaskReaction_SetMotorManual(MOTOR_LEVEL1);
			}
			else if (s_motor_cursor == 2U)
			{
				TaskReaction_SetMotorManual(MOTOR_LEVEL2);
			}
			else if (s_motor_cursor == 3U)
			{
				TaskReaction_SetMotorAuto();
			}
			else
			{
				s_ui_page = UI_PAGE_MENU;
			}
		}
		return;
	}

	if (s_ui_page == UI_PAGE_LIGHT)
	{
		if (key == KEY_1)
		{
			s_light_cursor = (s_light_cursor == 0U) ? 4U : (uint8_t)(s_light_cursor - 1U);
		}
		else if (key == KEY_3)
		{
			s_light_cursor = (uint8_t)((s_light_cursor + 1U) % 5U);
		}
		else if (key == KEY_2)
		{
			if (s_light_cursor == 0U)
			{
				TaskReaction_SetLightManual(0);
			}
			else if (s_light_cursor == 1U)
			{
				TaskReaction_SetLightManual(1);
			}
			else if (s_light_cursor == 2U)
			{
				TaskReaction_SetLightManual(2);
			}
			else if (s_light_cursor == 3U)
			{
				TaskReaction_SetLightAuto();
			}
			else
			{
				s_ui_page = UI_PAGE_MENU;
			}
		}
		return;
	}

	if (s_ui_page == UI_PAGE_HUMIDIFIER)
	{
		if (key == KEY_1)
		{
			s_humi_cursor = (s_humi_cursor == 0U) ? 3U : (uint8_t)(s_humi_cursor - 1U);
		}
		else if (key == KEY_3)
		{
			s_humi_cursor = (uint8_t)((s_humi_cursor + 1U) % 4U);
		}
		else if (key == KEY_2)
		{
			if (s_humi_cursor == 0U)
			{
				TaskReaction_SetHumidifierManual(HUMIDIFIER_ON);
			}
			else if (s_humi_cursor == 1U)
			{
				TaskReaction_SetHumidifierManual(HUMIDIFIER_OFF);
			}
			else if (s_humi_cursor == 2U)
			{
				TaskReaction_SetHumidifierAuto();
			}
			else
			{
				s_ui_page = UI_PAGE_MENU;
			}
		}
	}
}

/* 根据当前页面分发渲染函数 */
static void Ui_RenderCurrentPage(void)
{
	if (s_ui_page == UI_PAGE_HOME)
	{
		Ui_RenderHome();
	}
	else if (s_ui_page == UI_PAGE_MENU)
	{
		Ui_RenderMenu();
	}
	else if (s_ui_page == UI_PAGE_MOTOR)
	{
		Ui_RenderMotorPage();
	}
	else if (s_ui_page == UI_PAGE_LIGHT)
	{
		Ui_RenderLightPage();
	}
	else
	{
		Ui_RenderHumidifierPage();
	}
}

/* OLED UI任务初始化 */
void TaskOledUi_Init(void)
{
	OLED_Init();
	Key_Init();
	OLED_Clear();
	s_ui_page = UI_PAGE_HOME;
}

/* OLED+按键任务：页面渲染 + 按键事件处理 */
void Task_OledUi(void *pvParameters)
{
	uint8_t key;

	(void)pvParameters;
	TaskOledUi_Init();

	for (;;)
	{
		key = Key_GetNum();
		Ui_HandleKey(key);
		Ui_RenderCurrentPage();
		vTaskDelay(pdMS_TO_TICKS(OLED_UI_PERIOD_MS));
	}
}
