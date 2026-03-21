#include "stm32f10x.h"                  // Device header
#include "AT24C02.h"
#include "Delay.h"

#define I2C_TIMEOUT  ((uint32_t)0x2000)

/**
 * @brief  超时等待I2C事件完成
 * @param  I2Cx      I2C外设实例
 * @param  I2C_EVENT 目标事件
 * @return 0:事件到达 1:超时
 */
static uint8_t AT24C_WaitEvent(I2C_TypeDef* I2Cx, uint32_t I2C_EVENT)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (I2C_CheckEvent(I2Cx, I2C_EVENT) != SUCCESS)
    {
        if ((timeout--) == 0)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief  AT24C02写周期等待（无RTOS依赖）
 * @note   EEPROM内部擦写典型约5ms，使用阻塞延时保证下一次访问可靠。
 */
static void AT24C_WaitWriteCycle(void)
{
    Delay_ms(5);
}

/**
 * @brief  单次页写事务（不跨页）
 * @param  WordAddress 页内起始地址
 * @param  pData       数据指针
 * @param  length      写入长度（1~AT24C_PAGE_SIZE）
 * @return 0:成功 1:失败
 */
static uint8_t AT24C_WritePage(uint8_t WordAddress, const uint8_t *pData, uint8_t length)
{
    uint8_t i;

    I2C_GenerateSTART(AT24C_I2C, ENABLE);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_MODE_SELECT))
    {
        goto write_page_err;
    }

    I2C_Send7bitAddress(AT24C_I2C, AT24C_ADDR, I2C_Direction_Transmitter);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        goto write_page_err;
    }

    I2C_SendData(AT24C_I2C, WordAddress);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        goto write_page_err;
    }

    for (i = 0; i < length; i++)
    {
        I2C_SendData(AT24C_I2C, pData[i]);
        if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        {
            goto write_page_err;
        }
    }

    I2C_GenerateSTOP(AT24C_I2C, ENABLE);
    AT24C_WaitWriteCycle();
    return 0;

write_page_err:
    I2C_GenerateSTOP(AT24C_I2C, ENABLE);
    return 1;
}

/**
 * @brief  AT24C02 硬件初始化 (I2C1)
 */
void AT24C_Init(void)
{
    
    RCC_APB2PeriphClockCmd(AT24C_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(AT24C_I2C_CLK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Pin = AT24C_SCL_PIN | AT24C_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(AT24C_GPIO, &GPIO_InitStructure);

    I2C_DeInit(AT24C_I2C);

    I2C_InitTypeDef I2C_InitStructure;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;

    I2C_Init(AT24C_I2C, &I2C_InitStructure);
    I2C_Cmd(AT24C_I2C, ENABLE);

}

/**
 * @brief  写一个字节到指定地址
 * @param  WordAddress 地址(0x00-0xFF)
 * @param  Data        写入数据
 * @return 0:成功 1:失败
 */
uint8_t AT24C_WriteByte(uint8_t WordAddress, uint8_t Data)
{
    I2C_GenerateSTART(AT24C_I2C, ENABLE);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_MODE_SELECT))
    {
        goto write_err;
    }

    I2C_Send7bitAddress(AT24C_I2C, AT24C_ADDR, I2C_Direction_Transmitter);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        goto write_err;
    }

    I2C_SendData(AT24C_I2C, WordAddress);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        goto write_err;
    }

    I2C_SendData(AT24C_I2C, Data);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        goto write_err;
    }

    I2C_GenerateSTOP(AT24C_I2C, ENABLE);
    AT24C_WaitWriteCycle();
    return 0;

write_err:
    I2C_GenerateSTOP(AT24C_I2C, ENABLE);
    return 1;
}

/**
 * @brief  从指定地址读一个字节
 * @param  WordAddress 地址(0x00-0xFF)
 * @return 读取到的数据
 * @note   若总线异常，当前实现返回0（与有效数据0无法区分）。
 */
uint8_t AT24C_ReadByte(uint8_t WordAddress)
{
    uint8_t Data = 0;

    I2C_GenerateSTART(AT24C_I2C, ENABLE);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_MODE_SELECT))
    {
        goto read_err;
    }

    I2C_Send7bitAddress(AT24C_I2C, AT24C_ADDR, I2C_Direction_Transmitter);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        goto read_err;
    }

    I2C_SendData(AT24C_I2C, WordAddress);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        goto read_err;
    }

    I2C_GenerateSTART(AT24C_I2C, ENABLE);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_MODE_SELECT))
    {
        goto read_err;
    }

    I2C_Send7bitAddress(AT24C_I2C, AT24C_ADDR, I2C_Direction_Receiver);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
        goto read_err;
    }

    I2C_AcknowledgeConfig(AT24C_I2C, DISABLE);
    I2C_GenerateSTOP(AT24C_I2C, ENABLE);

    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED))
    {
        goto read_err_ack_restore;
    }
    Data = I2C_ReceiveData(AT24C_I2C);

    I2C_AcknowledgeConfig(AT24C_I2C, ENABLE);
    return Data;

read_err_ack_restore:
    I2C_AcknowledgeConfig(AT24C_I2C, ENABLE);
read_err:
    I2C_GenerateSTOP(AT24C_I2C, ENABLE);
    return 0;
}

/**
 * @brief  连续写入数据（真正页写，自动跨页）
 * @param  WordAddress 起始地址(0x00-0xFF)
 * @param  pData       数据缓冲区
 * @param  length      写入长度
 * @return 0:成功 1:失败
 * @note   每次I2C事务最多写入一页内的数据（AT24C02页大小8字节），
 *         超过页边界会自动拆分为多次页写事务。
 */
uint8_t AT24C_WriteBuffer(uint8_t WordAddress, const uint8_t *pData, uint16_t length)
{
    uint16_t remaining;
    uint16_t current_addr;
    uint8_t page_remain;
    uint8_t chunk;
    const uint8_t *p_cur;

    if ((pData == 0) || (length == 0))
    {
        return 0;
    }

    if (((uint16_t)WordAddress + length) > AT24C_TOTAL_SIZE)
    {
        return 1;
    }

    remaining = length;
    current_addr = WordAddress;
    p_cur = pData;

    while (remaining > 0)
    {
        page_remain = (uint8_t)(AT24C_PAGE_SIZE - (current_addr % AT24C_PAGE_SIZE));
        chunk = (remaining > page_remain) ? page_remain : (uint8_t)remaining;

        if (AT24C_WritePage((uint8_t)current_addr, p_cur, chunk) != 0)
        {
            return 1;
        }

        current_addr += chunk;
        p_cur += chunk;
        remaining -= chunk;
    }

    return 0;
}

/**
 * @brief  连续读取数据
 * @param  WordAddress 起始地址(0x00-0xFF)
 * @param  pData       读取缓冲区
 * @param  length      读取长度
 * @return 0:成功 1:失败
 */
uint8_t AT24C_ReadBuffer(uint8_t WordAddress, uint8_t *pData, uint16_t length)
{
    uint16_t i;

    if ((pData == 0) || (length == 0))
    {
        return 0;
    }

    for (i = 0; i < length; i++)
    {
        pData[i] = AT24C_ReadByte((uint8_t)(WordAddress + i));
    }

    return AT24C_CheckDevice();
}

/**
 * @brief  检查器件是否存在（用于系统自检）
 * @return 0:正常 1:异常
 */
uint8_t AT24C_CheckDevice(void)
{
    uint8_t status = 0;

    I2C_GenerateSTART(AT24C_I2C, ENABLE);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_MODE_SELECT))
    {
        status = 1;
    }

    I2C_Send7bitAddress(AT24C_I2C, AT24C_ADDR, I2C_Direction_Transmitter);
    if (AT24C_WaitEvent(AT24C_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        status = 1;
    }

    I2C_GenerateSTOP(AT24C_I2C, ENABLE);
    return status;
}
