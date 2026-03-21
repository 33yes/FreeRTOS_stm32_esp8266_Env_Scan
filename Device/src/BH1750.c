#include "stm32f10x.h"                  // Device header
#include "BH1750.h"
#include "Delay.h"

#define I2C_TIMEOUT  ((uint32_t)0x2000)
#define BH1750_MEASURE_WAIT_MS  180u
#define BH1750_FILTER_SAMPLES   5u

/**
 * @brief  等待硬件I2C事件，带超时退出防止任务卡死
 */
static uint8_t BH1750_WaitEvent(I2C_TypeDef* I2Cx, uint32_t I2C_EVENT)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (I2C_CheckEvent(I2Cx, I2C_EVENT) != SUCCESS)
    {
        if ((timeout--) == 0) 
				return 1; // 超时返回错误
    }
    return 0;
}

/**
 * @brief  向BH1750写入单字节命令
 */
static uint8_t BH1750_Write_Cmd(uint8_t cmd) 
{
    I2C_GenerateSTART(BH1750_I2C, ENABLE);
    if(BH1750_WaitEvent(BH1750_I2C, I2C_EVENT_MASTER_MODE_SELECT)) 
    goto write_cmd_err;

    I2C_Send7bitAddress(BH1750_I2C, BH1750_ADDR, I2C_Direction_Transmitter);
    if(BH1750_WaitEvent(BH1750_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) 
    goto write_cmd_err;

    I2C_SendData(BH1750_I2C, cmd);
    if(BH1750_WaitEvent(BH1750_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) 
    goto write_cmd_err;

    I2C_GenerateSTOP(BH1750_I2C, ENABLE);
    return 0;

write_cmd_err:
  I2C_GenerateSTOP(BH1750_I2C, ENABLE);
  return 1;
}

/**
 * @brief  读取一次BH1750原始数据（2字节）
 * @param  raw_data 输出原始值
 * @return 0:成功 1:失败
 */
static uint8_t BH1750_Read_Raw(uint16_t *raw_data)
{
  uint16_t raw;

  if(raw_data == 0)
    return 1;

  I2C_GenerateSTART(BH1750_I2C, ENABLE);
  if(BH1750_WaitEvent(BH1750_I2C, I2C_EVENT_MASTER_MODE_SELECT))
    goto read_raw_err;

  I2C_Send7bitAddress(BH1750_I2C, BH1750_ADDR, I2C_Direction_Receiver);
  if(BH1750_WaitEvent(BH1750_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    goto read_raw_err;

  if(BH1750_WaitEvent(BH1750_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED))
    goto read_raw_err;
  raw = ((uint16_t)I2C_ReceiveData(BH1750_I2C)) << 8;

  I2C_AcknowledgeConfig(BH1750_I2C, DISABLE);
  I2C_GenerateSTOP(BH1750_I2C, ENABLE);

  if(BH1750_WaitEvent(BH1750_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED))
    goto read_raw_err_ack_restore;
  raw |= I2C_ReceiveData(BH1750_I2C);

  I2C_AcknowledgeConfig(BH1750_I2C, ENABLE);
  *raw_data = raw;
  return 0;

read_raw_err_ack_restore:
  I2C_AcknowledgeConfig(BH1750_I2C, ENABLE);
read_raw_err:
  I2C_GenerateSTOP(BH1750_I2C, ENABLE);
  return 1;
}

/**
 * @brief  BH1750 硬件初始化
 */
void BH1750_Init(void)
{
    RCC_APB2PeriphClockCmd(BH1750_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(BH1750_I2C_CLK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD; // 复用开漏
    GPIO_InitStructure.GPIO_Pin = BH1750_SCL_PIN | BH1750_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BH1750_GPIO, &GPIO_InitStructure);
    
    I2C_DeInit(BH1750_I2C); // 复位外设，清除上次可能的挂死状态
    
    I2C_InitTypeDef I2C_InitStructure;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000; // 100kHz
    
    I2C_Init(BH1750_I2C, &I2C_InitStructure);
    I2C_Cmd(BH1750_I2C, ENABLE);
}

/**
 * @brief  配置BH1750测量模式
 */
void BH1750_Init_Config(void) 
{
  BH1750_Write_Cmd(BH1750_CMD_POWER_ON);
  BH1750_Write_Cmd(BH1750_CMD_RESET);
  BH1750_Write_Cmd(BH1750_CMD_H_RES_MODE);
  Delay_ms(BH1750_MEASURE_WAIT_MS);
}

/**
 * @brief  读取光照强度值（去极值平均滤波）
 * @note   连续读取5次，丢弃1个最大值与1个最小值，再求平均；
 *         算法简单且对偶发抖动有较好抑制效果。
 */
float BH1750_Read_Lux(void) 
{
  uint8_t i;
  uint16_t raw;
  uint32_t sum;
  uint16_t min_v;
  uint16_t max_v;
  uint16_t samples[BH1750_FILTER_SAMPLES];

  for(i = 0; i < BH1750_FILTER_SAMPLES; i++)
  {
    if(BH1750_Read_Raw(&raw) != 0)
      return 0.0f;
    samples[i] = raw;
    Delay_ms(2);
  }

  sum = samples[0];
  min_v = samples[0];
  max_v = samples[0];
  for(i = 1; i < BH1750_FILTER_SAMPLES; i++)
  {
    sum += samples[i];
    if(samples[i] < min_v)
      min_v = samples[i];
    if(samples[i] > max_v)
      max_v = samples[i];
  }

  sum -= min_v;
  sum -= max_v;
  raw = (uint16_t)(sum / (BH1750_FILTER_SAMPLES - 2));

  return (float)raw / 1.2f; 
}
