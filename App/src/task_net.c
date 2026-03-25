#include "task_net.h"
#include "task_sensor.h"
#include "esp8266.h"
#include "usart2.h"
#include "LED.h"
#include "motor.h"
#include "humidity.h"
#include "delay.h"
#include "string.h"
#include "stdio.h"

/*---------------------------------------------------------------*/
/* 文件说明：task_net.c                                          */
/* 功  能：网络任务（ESP8266 + MQTT，当前对接OneNet）            */
/* 内  容：                                                       */
/*  1. WiFi连接与重连                                             */
/*  2. MQTT报文收发泵处理                                         */
/*  3. OneNet阈值下发解析与持久化（通过mqtt.c接口）               */
/*  4. 周期性上报网络状态和阈值                                   */
/*---------------------------------------------------------------*/

#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#define TASK_NET_WEAK __weak
#elif defined(__GNUC__)
#define TASK_NET_WEAK __attribute__((weak))
#else
#define TASK_NET_WEAK
#endif

/* 任务周期与网络策略参数 */
#define NET_MQTT_TASK_PERIOD_MS         20
#define NET_MQTT_RECONNECT_DELAY_MS     3000
#define NET_MQTT_CONNACK_TIMEOUT_MS     10000
#define NET_MQTT_PING_INTERVAL_MS       30000
#define NET_MQTT_PING_TIMEOUT_MS        10000
#define NET_MQTT_REPORT_INTERVAL_MS     15000

#define NET_MQTT_MAX_TX_PUMP_PER_ROUND  4
#define NET_MQTT_MAX_RX_PUMP_PER_ROUND  4

TASK_NET_WEAK const char SSID[] = "YOUR_WIFI_SSID";  //热点名称
TASK_NET_WEAK const char PASS[] = "YOUR_WIFI_PASS";  //热点密码
TASK_NET_WEAK const char PRODUCTID[] = "80id181j0o";  //OneNet产品ID
TASK_NET_WEAK const char DEVICEID[] = "Env_001";  //OneNet设备ID
TASK_NET_WEAK const char AUTHENTICATION[] = "RjVDMzJSZVZ1emZZeWVLbEh5U3JlUHI2V1hFUEt5anU=";  //API key
TASK_NET_WEAK const char DATA_TOPIC_NAME[] = "$dp";
// MQTT 固定地址
TASK_NET_WEAK const char SERVER_IP[] = "183.230.40.39";
TASK_NET_WEAK const int SERVER_PORT = 6002;

TASK_NET_WEAK const char CMD_TOPIC_NAME[] = "$creq/#"; //OneNet下行命令Topic

/* 网络运行时状态 */
NetMqttRuntime_t g_net_mqtt_runtime = {
	NET_MQTT_STATE_IDLE,
	0,
	0,
	0,
	0,
	0,
	0
};

/* 记录进入“等待CONNACK”状态的时间戳，用于连接超时判断 */
static TickType_t s_connack_wait_start_tick = 0;

#define NET_MQTT_CONNACK_TIMEOUT_TICKS    pdMS_TO_TICKS(NET_MQTT_CONNACK_TIMEOUT_MS)
#define NET_MQTT_RECONNECT_DELAY_TICKS    pdMS_TO_TICKS(NET_MQTT_RECONNECT_DELAY_MS)
#define NET_MQTT_PING_INTERVAL_TICKS      pdMS_TO_TICKS(NET_MQTT_PING_INTERVAL_MS)
#define NET_MQTT_PING_TIMEOUT_TICKS       pdMS_TO_TICKS(NET_MQTT_PING_TIMEOUT_MS)
#define NET_MQTT_REPORT_INTERVAL_TICKS    pdMS_TO_TICKS(NET_MQTT_REPORT_INTERVAL_MS)
#define NET_MQTT_TASK_PERIOD_TICKS        pdMS_TO_TICKS(NET_MQTT_TASK_PERIOD_MS)

/* 回到空闲态并按需复位WiFi模块 */
static void NetMqtt_EnterIdle(uint8_t do_wifi_reset)
{
	g_net_mqtt_runtime.wifi_connected = 0;
	g_net_mqtt_runtime.mqtt_connected = 0;
	g_net_mqtt_runtime.state = NET_MQTT_STATE_IDLE;
	g_net_mqtt_runtime.reconnect_count++;

	if (do_wifi_reset)
	{
		WiFi_Reset(20);
	}

	vTaskDelay(NET_MQTT_RECONNECT_DELAY_TICKS);
}

/* 处理心跳与周期上报定时逻辑 */
static void NetMqtt_CheckPingAndReport(TickType_t now)
{
	if ((now - g_net_mqtt_runtime.last_ping_tick) > NET_MQTT_PING_INTERVAL_TICKS)
	{
		MQTT_PingREQ();
		pingFlag = 1;
		g_net_mqtt_runtime.last_ping_tick = now;
	}

	if ((pingFlag == 1) && ((now - g_net_mqtt_runtime.last_ping_tick) > NET_MQTT_PING_TIMEOUT_TICKS))
	{
		NetMqtt_EnterIdle(1);
		return;
	}

	if ((now - g_net_mqtt_runtime.last_report_tick) > NET_MQTT_REPORT_INTERVAL_TICKS)
	{
		TaskNetMqtt_PublishStatusAndThreshold();
	}
}

/*---------------------------------------------------------------*/
/*函数名：NetMqtt_AdvanceTxPtr                                    */
/*功  能：发送缓冲区读指针下移并处理回环                          */
/*参  数：无                                                      */
/*返回值：无                                                      */
/*---------------------------------------------------------------*/
static void NetMqtt_AdvanceTxPtr(void)
{
	MQTT_TxDataOutPtr += TBUFF_UNIT;
	if (MQTT_TxDataOutPtr == MQTT_TxDataEndPtr)
	{
		MQTT_TxDataOutPtr = MQTT_TxDataBuf[0];
	}
}

/*---------------------------------------------------------------*/
/*函数名：NetMqtt_AdvanceRxPtr                                    */
/*功  能：接收缓冲区读指针下移并处理回环                          */
/*参  数：无                                                      */
/*返回值：无                                                      */
/*---------------------------------------------------------------*/
static void NetMqtt_AdvanceRxPtr(void)
{
	MQTT_RxDataOutPtr += RBUFF_UNIT;
	if (MQTT_RxDataOutPtr == MQTT_RxDataEndPtr)
	{
		MQTT_RxDataOutPtr = MQTT_RxDataBuf[0];
	}
}

/*---------------------------------------------------------------*/
/*函数名：NetMqtt_HandleConnAck                                   */
/*功  能：处理MQTT连接应答（CONNACK）                             */
/*参  数：rx_frame：接收缓冲区中的完整报文                        */
/*返回值：无                                                      */
/*说  明：连接成功后切换到运行态并订阅下行Topic                   */
/*---------------------------------------------------------------*/
static void NetMqtt_HandleConnAck(const unsigned char *rx_frame)
{
	if (rx_frame[5] == 0x00)
	{
		g_net_mqtt_runtime.mqtt_connected = 1;
		g_net_mqtt_runtime.state = NET_MQTT_STATE_RUNNING;
		g_net_mqtt_runtime.last_pong_tick = xTaskGetTickCount();
		g_net_mqtt_runtime.last_ping_tick = xTaskGetTickCount();
		g_net_mqtt_runtime.last_report_tick = 0;

		MQTT_Subscribe((char *)CMD_TOPIC_NAME, 0);
	}
	else
	{
		NetMqtt_EnterIdle(0);
	}
}

/*---------------------------------------------------------------*/
/*函数名：NetMqtt_ProcessCmdQueue                                 */
/*功  能：处理命令缓冲区中的云端下发命令                          */
/*参  数：无                                                      */
/*返回值：无                                                      */
/*说  明：当前主要处理阈值修改命令，成功后立即回报最新阈值         */
/*---------------------------------------------------------------*/
static void NetMqtt_ProcessCmdQueue(void)
{
	while (MQTT_CMDOutPtr != MQTT_CMDInPtr)
	{
		if (MQTT_ProcessOneNetThresholdCommand())
		{
			TaskNetMqtt_PublishStatusAndThreshold();
		}
	}
}

/*---------------------------------------------------------------*/
/*函数名：NetMqtt_PumpTx                                           */
/*功  能：分批发送MQTT发送缓冲区中的报文                          */
/*参  数：无                                                      */
/*返回值：无                                                      */
/*说  明：每轮限制处理数量，避免网络任务长期占用CPU               */
/*---------------------------------------------------------------*/
static void NetMqtt_PumpTx(void)
{
	uint8_t count;

	count = 0;
	while ((MQTT_TxDataOutPtr != MQTT_TxDataInPtr) && (count < NET_MQTT_MAX_TX_PUMP_PER_ROUND))
	{
		MQTT_TxData(MQTT_TxDataOutPtr);
		NetMqtt_AdvanceTxPtr();
		count++;
	}
}

/*---------------------------------------------------------------*/
/*函数名：NetMqtt_PumpRx                                           */
/*功  能：分批处理MQTT接收缓冲区中的报文                          */
/*参  数：无                                                      */
/*返回值：无                                                      */
/*说  明：识别CONNACK/PUBLISH/PINGRESP并分发处理                  */
/*---------------------------------------------------------------*/
static void NetMqtt_PumpRx(void)
{
	uint8_t count;

	count = 0;
	while ((MQTT_RxDataOutPtr != MQTT_RxDataInPtr) && (count < NET_MQTT_MAX_RX_PUMP_PER_ROUND))
	{
		uint8_t type = MQTT_RxDataOutPtr[2];

		if (type == 0x20)
		{
			NetMqtt_HandleConnAck(MQTT_RxDataOutPtr);
		}
		else if (type == 0x30)
		{
			MQTT_DealPushdata_Qs0(MQTT_RxDataOutPtr);
			NetMqtt_ProcessCmdQueue();
		}
		else if (type == 0xD0)
		{
			pingFlag = 0;
			g_net_mqtt_runtime.last_pong_tick = xTaskGetTickCount();
		}

		NetMqtt_AdvanceRxPtr();
		count++;
	}
}

/*---------------------------------------------------------------*/
/*函数名：NetMqtt_TryConnect                                       */
/*功  能：尝试连接WiFi和云服务器                                  */
/*参  数：无                                                      */
/*返回值：无                                                      */
/*说  明：连接成功后初始化MQTT缓冲并进入等待CONNACK状态           */
/*---------------------------------------------------------------*/
static void NetMqtt_TryConnect(void)
{
	if (WiFi_Connect_IoTServer() == 0)
	{
		g_net_mqtt_runtime.wifi_connected = 1;
		g_net_mqtt_runtime.state = NET_MQTT_STATE_MQTT_WAIT_CONNACK;

		MQTT_Buff_Init();
		pingFlag = 0;

		s_connack_wait_start_tick = xTaskGetTickCount();
	}
	else
	{
		NetMqtt_EnterIdle(0);
	}
}

/*---------------------------------------------------------------*/
/*函数名：TaskNetMqtt_PublishStatusAndThreshold                   */
/*功  能：上报网络状态、阈值、传感器参数和设备状态到云端          */
/*参  数：无                                                      */
/*返回值：pdPASS/pdFAIL                                           */
/*---------------------------------------------------------------*/
BaseType_t TaskNetMqtt_PublishStatusAndThreshold(void)
{
	char payload[512];
	int payload_len;
	SensorData_t sensor_snapshot;
	BaseType_t has_sensor_data;
	uint8_t led_mode;
	Motor_Speed_e motor_state;
	Humidifier_State_e humidifier_state;

	/* 读取各设备当前状态（无数据时传0） */
	has_sensor_data = TaskSensor_GetLatest(&sensor_snapshot);
	if (has_sensor_data != pdPASS)
	{
		memset(&sensor_snapshot, 0, sizeof(sensor_snapshot));
	}

	led_mode = LED_GetBrightness();
	motor_state = Motor_GetSpeed();
	humidifier_state = Humidifier_GetState();

	payload_len = snprintf(
		payload,
		sizeof(payload),
		"{\"id\":\"1\",\"version\":\"1.0\",\"params\":{"
		"temperature_threshold\":{\"value\":%d},"
		"humidity_threshold\":{\"value\":%d},"
		"light_threshold\":{\"value\":%u},"
		"net_state\":{\"value\":%d},"
		"wifi_state\":{\"value\":%d},"
		"temperature\":{\"value\":%d},"
		"humidity\":{\"value\":%d},"
		"light\":{\"value\":%u},"
		"motion\":{\"value\":%u},"
		"led_state\":{\"value\":%u},"
		"motor_state\":{\"value\":%d},"
		"humidifier_state\":{\"value\":%d}}}",
		g_mqtt_threshold.temperature_threshold,
		g_mqtt_threshold.humidity_threshold,
		g_mqtt_threshold.light_threshold,
		(int)g_net_mqtt_runtime.mqtt_connected,
		(int)g_net_mqtt_runtime.wifi_connected,
		(int)sensor_snapshot.temperature,
		(int)sensor_snapshot.humidity,
		(unsigned int)sensor_snapshot.light,
		(unsigned int)sensor_snapshot.motion,
		(unsigned int)led_mode,
		(int)motor_state,
		(int)humidifier_state
	);

	if ((payload_len <= 0) || (payload_len >= (int)sizeof(payload)))
	{
		return pdFAIL;
	}

	taskENTER_CRITICAL();
	/* OneNet上报：将JSON封装为$dp格式后入MQTT发送缓冲区 */
	if (MQTT_PublishOneNetDpJson(payload, payload_len) != 0)
	{
		taskEXIT_CRITICAL();
		return pdFAIL;
	}
	taskEXIT_CRITICAL();

	g_net_mqtt_runtime.last_report_tick = xTaskGetTickCount();
	return pdPASS;
}

/*---------------------------------------------------------------*/
/*函数名：TaskNetMqtt_IsConnected                                  */
/*功  能：查询MQTT是否已连接                                       */
/*参  数：无                                                      */
/*返回值：pdPASS=已连接，pdFAIL=未连接                            */
/*---------------------------------------------------------------*/
BaseType_t TaskNetMqtt_IsConnected(void)
{
	if (g_net_mqtt_runtime.mqtt_connected)
	{
		return pdPASS;
	}

	return pdFAIL;
}

/*---------------------------------------------------------------*/
/*函数名：TaskNetMqtt_GetRuntime                                   */
/*功  能：获取网络任务运行状态结构体指针                          */
/*参  数：无                                                      */
/*返回值：状态结构体只读指针                                       */
/*---------------------------------------------------------------*/
const NetMqttRuntime_t *TaskNetMqtt_GetRuntime(void)
{
	return &g_net_mqtt_runtime;
}

/*---------------------------------------------------------------*/
/*函数名：Task_NetMqtt                                             */
/*功  能：网络核心任务                                             */
/*流  程：                                                         */
/*  1. 初始化串口2、ESP8266复位脚、云参数与阈值                   */
/*  2. 空闲态：尝试连接WiFi和服务器                                */
/*  3. 等待CONNACK态：泵发送/接收并做超时处理                      */
/*  4. 运行态：收发处理、心跳维持、下行命令处理、周期状态上报       */
/*参  数：pvParameters：任务参数（未使用）                         */
/*返回值：无（FreeRTOS任务函数）                                   */
/*---------------------------------------------------------------*/
void Task_NetMqtt(void *pvParameters)
{
	TickType_t now;

	(void)pvParameters;

	usart2_init(115200);
	wifi_reset_io_init();
	IoT_parameter_init();
	MQTT_ThresholdInit();

	g_net_mqtt_runtime.state = NET_MQTT_STATE_IDLE;
	g_net_mqtt_runtime.mqtt_connected = 0;
	g_net_mqtt_runtime.wifi_connected = 0;

	for (;;)
	{
		now = xTaskGetTickCount();

		if (g_net_mqtt_runtime.state == NET_MQTT_STATE_IDLE)
		{
			NetMqtt_TryConnect();
		}
		else if (g_net_mqtt_runtime.state == NET_MQTT_STATE_MQTT_WAIT_CONNACK)
		{
			NetMqtt_PumpTx();
			NetMqtt_PumpRx();

			if ((xTaskGetTickCount() - s_connack_wait_start_tick) > NET_MQTT_CONNACK_TIMEOUT_TICKS)
			{
				NetMqtt_EnterIdle(0);
			}
		}
		else if (g_net_mqtt_runtime.state == NET_MQTT_STATE_RUNNING)
		{
			NetMqtt_PumpTx();
			NetMqtt_PumpRx();
			NetMqtt_ProcessCmdQueue();
			NetMqtt_CheckPingAndReport(now);
		}

		vTaskDelay(NET_MQTT_TASK_PERIOD_TICKS);
	}
}
