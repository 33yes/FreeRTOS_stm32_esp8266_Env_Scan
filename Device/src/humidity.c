#include "stm32f10x.h"
#include "humidity.h"

/* ======================== 全局状态变量 ======================== */
static Humidifier_State_e g_Humidifier_State = HUMIDIFIER_OFF; /*!< 加湿器当前状态 */

/**
  * @brief  初始化加湿器控制引脚 (BIN1/BIN2)，加湿器不需要PWM控制加湿速率
  */
void Humidifier_Init(void)
{
    /* BIN1 和 BIN2 初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(HUMI_GPIO_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = HUMI_BIN1_PIN | HUMI_BIN2_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HUMI_GPIO, &GPIO_InitStructure);
    /* 初始状态：关闭 */
    GPIO_ResetBits(HUMI_GPIO, HUMI_BIN1_PIN);
    GPIO_ResetBits(HUMI_GPIO, HUMI_BIN2_PIN);
}

/**
  * @brief  控制加湿器开关
  * @param  State: 
  *         - HUMIDIFIER_OFF (0) : 关闭
  *         - HUMIDIFIER_ON  (1) : 开启
  */
void Humidifier_Control(Humidifier_State_e State)
{
    if (State == HUMIDIFIER_ON)
    {
        /* 开启：BIN1=1, BIN2=0（H桥高电位驱动）*/
        GPIO_SetBits(HUMI_GPIO, HUMI_BIN1_PIN);
        GPIO_ResetBits(HUMI_GPIO, HUMI_BIN2_PIN);
        g_Humidifier_State = HUMIDIFIER_ON;
    }
    else
    {
        /* 关闭：两个引脚都拉低 */
        GPIO_ResetBits(HUMI_GPIO, HUMI_BIN1_PIN);
        GPIO_ResetBits(HUMI_GPIO, HUMI_BIN2_PIN);
        g_Humidifier_State = HUMIDIFIER_OFF;
    }
}

/**
  * @brief  获取加湿器当前状态
  * @return Humidifier_State_e 
  */
Humidifier_State_e Humidifier_GetState(void)
{
    return g_Humidifier_State;
}
