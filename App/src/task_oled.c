#include "task_oled.h"

#include "OLED.h"
#include "Key.h"
#include "task_sensor.h"
#include "task_net.h"
#include "task_reaction.h"
#include "stdio.h"
#include "string.h"

#define OLED_UI_PERIOD_MS    150
#define OLED_LINE_CHARS      16

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
static uint8_t s_menu_scroll_top = 0;
static uint8_t s_motor_cursor = 0;
static uint8_t s_light_cursor = 0;
static uint8_t s_humi_cursor = 0;

/* 每行显示缓存：用于“仅更新变化行”，减少闪屏 */
static char s_oled_line_cache[4][OLED_LINE_CHARS + 1];

/* 将文本裁剪/补空格到16字符，并仅在内容变化时刷新该行 */
static void Ui_ShowLineCached(uint8_t line, const char *text)
{
	char out[OLED_LINE_CHARS + 1];
	uint8_t i;

	for (i = 0; i < OLED_LINE_CHARS; i++)
	{
		out[i] = ' ';
	}
	out[OLED_LINE_CHARS] = '\0';

	if (text != NULL)
	{
		for (i = 0; (i < OLED_LINE_CHARS) && (text[i] != '\0'); i++)
		{
			out[i] = text[i];
		}
	}

	if (strncmp(s_oled_line_cache[line - 1U], out, OLED_LINE_CHARS) != 0)
	{
		strncpy(s_oled_line_cache[line - 1U], out, OLED_LINE_CHARS + 1U);
		OLED_ShowString(line, 1, out);
	}
}

/* 标记整页脏区：用于翻页后强制刷新4行 */
static void Ui_InvalidateAllLines(void)
{
	uint8_t i;
	for (i = 0; i < 4U; i++)
	{
		s_oled_line_cache[i][0] = '\0';
	}
}

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
	const NetMqttRuntime_t *net;

	(void)TaskSensor_GetLatest(&sensor);
	net = TaskNetMqtt_GetRuntime();

	snprintf(line, sizeof(line), "esp8266:%s", (net->wifi_connected != 0U) ? "ON" : "OFF");
	Ui_ShowLineCached(1, line);

	snprintf(line, sizeof(line), "temp:%d", (int)sensor.temperature);
	Ui_ShowLineCached(2, line);

	snprintf(line, sizeof(line), "humi:%d", (int)sensor.humidity);
	Ui_ShowLineCached(3, line);

	snprintf(line, sizeof(line), "light:%u", (unsigned int)sensor.light);
	Ui_ShowLineCached(4, line);
}

/* 二级菜单：无按键提示；当条目超过可显示行时自动滚动 */
static void Ui_RenderMenu(void)
{
	char line[24];
	uint8_t i;
	static const char *menu_items[] = {"MOTOR", "LIGHT", "HUMID", "BACK"};

	/* 第1行显示标题，第2~4行显示选项，因此可见窗口大小=3 */
	if (s_menu_cursor < s_menu_scroll_top)
	{
		s_menu_scroll_top = s_menu_cursor;
	}
	else if (s_menu_cursor >= (uint8_t)(s_menu_scroll_top + 3U))
	{
		s_menu_scroll_top = (uint8_t)(s_menu_cursor - 2U);
	}

	Ui_ShowLineCached(1, "menu");

	for (i = 0; i < 3U; i++)
	{
		uint8_t idx = (uint8_t)(s_menu_scroll_top + i);
		if (idx < 4U)
		{
			snprintf(line, sizeof(line), "%c%s", (idx == s_menu_cursor) ? '>' : ' ', menu_items[idx]);
			Ui_ShowLineCached((uint8_t)(i + 2U), line);
		}
		else
		{
			Ui_ShowLineCached((uint8_t)(i + 2U), "");
		}
	}
}

/* 电机控制页面渲染 */
static void Ui_RenderMotorPage(void)
{
	char line[24];
	ReactionStatus_t react;
	(void)TaskReaction_GetStatus(&react);

	snprintf(line, sizeof(line), "motor:%s", Ui_MotorStatusText(&react));
	Ui_ShowLineCached(1, line);
	snprintf(line, sizeof(line), "%cOFF %cL1", (s_motor_cursor == 0U) ? '>' : ' ', (s_motor_cursor == 1U) ? '>' : ' ');
	Ui_ShowLineCached(2, line);
	snprintf(line, sizeof(line), "%cL2  %cAUTO", (s_motor_cursor == 2U) ? '>' : ' ', (s_motor_cursor == 3U) ? '>' : ' ');
	Ui_ShowLineCached(3, line);
	snprintf(line, sizeof(line), "%cBACK", (s_motor_cursor == 4U) ? '>' : ' ');
	Ui_ShowLineCached(4, line);
}

/* 灯光控制页面渲染 */
static void Ui_RenderLightPage(void)
{
	char line[24];
	ReactionStatus_t react;
	(void)TaskReaction_GetStatus(&react);

	snprintf(line, sizeof(line), "light:%s", Ui_LightStatusText(&react));
	Ui_ShowLineCached(1, line);
	snprintf(line, sizeof(line), "%cOFF %cL1", (s_light_cursor == 0U) ? '>' : ' ', (s_light_cursor == 1U) ? '>' : ' ');
	Ui_ShowLineCached(2, line);
	snprintf(line, sizeof(line), "%cL2  %cAUTO", (s_light_cursor == 2U) ? '>' : ' ', (s_light_cursor == 3U) ? '>' : ' ');
	Ui_ShowLineCached(3, line);
	snprintf(line, sizeof(line), "%cBACK", (s_light_cursor == 4U) ? '>' : ' ');
	Ui_ShowLineCached(4, line);
}

/* 加湿器控制页面渲染 */
static void Ui_RenderHumidifierPage(void)
{
	char line[24];
	ReactionStatus_t react;
	(void)TaskReaction_GetStatus(&react);

	snprintf(line, sizeof(line), "humid:%s", Ui_HumidifierStatusText(&react));
	Ui_ShowLineCached(1, line);
	snprintf(line, sizeof(line), "%cON   %cOFF", (s_humi_cursor == 0U) ? '>' : ' ', (s_humi_cursor == 1U) ? '>' : ' ');
	Ui_ShowLineCached(2, line);
	snprintf(line, sizeof(line), "%cAUTO", (s_humi_cursor == 2U) ? '>' : ' ');
	Ui_ShowLineCached(3, line);
	snprintf(line, sizeof(line), "%cBACK", (s_humi_cursor == 3U) ? '>' : ' ');
	Ui_ShowLineCached(4, line);
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
			s_menu_scroll_top = 0;
			Ui_InvalidateAllLines();
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
				Ui_InvalidateAllLines();
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
				Ui_InvalidateAllLines();
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
				Ui_InvalidateAllLines();
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
				Ui_InvalidateAllLines();
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
	Ui_InvalidateAllLines();
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
