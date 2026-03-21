#include "stm32f10x.h"                  // Device header

#include "DHT11.h"
#include "Delay.h"

#define DHT11_START_LOW_MS          20u
#define DHT11_RESP_TIMEOUT_US       120u
#define DHT11_BIT_LOW_TIMEOUT_US    100u
#define DHT11_BIT_HIGH_TIMEOUT_US   100u
#define DHT11_FILTER_SAMPLES        3u

// 微秒级延时，利用简单的循环实现（DHT11对精度要求很高）
static void DHT11_Delay_us(uint16_t us) 
{
    uint32_t count = us * (SystemCoreClock / 1000000) / 5;
    while(count--);
}

// 等待引脚到达期望电平，带超时保护
static uint8_t DHT11_Wait_PinState(BitAction state, uint16_t timeout_us)
{
    while(timeout_us--)
    {
        if(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == state)
            return 0;
        DHT11_Delay_us(1);
    }
    return 1;
}

static uint8_t DHT11_Median3(uint8_t a, uint8_t b, uint8_t c)
{
    if(a > b)
    {
        uint8_t t = a;
        a = b;
        b = t;
    }
    if(b > c)
    {
        uint8_t t = b;
        b = c;
        c = t;
    }
    if(a > b)
    {
        uint8_t t = a;
        a = b;
        b = t;
    }
    return b;
}

// 读取一个位（0或1）
static uint8_t DHT11_Read_Bit(uint8_t *bit_value)
{
    if(DHT11_Wait_PinState(RESET, DHT11_BIT_LOW_TIMEOUT_US))
        return 1;
    if(DHT11_Wait_PinState(SET, DHT11_BIT_HIGH_TIMEOUT_US))
        return 1;

    DHT11_Delay_us(40); // 越过30us的阈值
    if(GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == SET) 
        *bit_value = 1;
    else 
        *bit_value = 0;

    if(DHT11_Wait_PinState(RESET, DHT11_BIT_HIGH_TIMEOUT_US))
        return 1;

    return 0;
}

/**
    * @brief  从DHT11读取一次“原始”温湿度数据（单次采样，不做滤波）
    * @param  DHT11_Data 输出温湿度
    * @return 0:成功 1:响应失败/超时 2:校验错误
    * @note   该函数只负责“单次时序采样 + 校验”，不负责数据平滑。
    *         这样可以把底层时序与上层滤波解耦，后续更换滤波算法时无需改动时序代码。
    */
static uint8_t DHT11_Read_Data_Raw(DHT11_Data_TypeDef *DHT11_Data)
{
    uint8_t buf[5] = {0};
    uint8_t i, j;
    uint8_t bit_value;
    uint8_t ret = 0;

    if(DHT11_Data == 0)
        return 1;

    DHT11_Mode_Out();
    GPIO_ResetBits(DHT11_GPIO, DHT11_PIN);
    Delay_ms(DHT11_START_LOW_MS);
    GPIO_SetBits(DHT11_GPIO, DHT11_PIN);
    DHT11_Delay_us(30);

    DHT11_Mode_In();
    if(DHT11_Wait_PinState(RESET, DHT11_RESP_TIMEOUT_US))
        return 1;
    if(DHT11_Wait_PinState(SET, DHT11_RESP_TIMEOUT_US))
        return 1;
    if(DHT11_Wait_PinState(RESET, DHT11_RESP_TIMEOUT_US))
        return 1;

    __disable_irq();
    for(i = 0; i < 5; i++)
    {
        for(j = 0; j < 8; j++)
        {
            buf[i] <<= 1;
            if(DHT11_Read_Bit(&bit_value))
            {
                ret = 1;
                goto exit_read;
            }
            if(bit_value)
                buf[i] |= 1;
        }
    }
exit_read:
    __enable_irq();

    if(ret)
        return 1;

    if((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
        return 2;

    DHT11_Data->hum = buf[0];
    DHT11_Data->temp = buf[2];
    return 0;
}

/**
    * @brief  从DHT11读取温湿度数据（对外接口，内部带轻量滤波）
    * @param  DHT11_Data 输出温湿度
    * @return 0:成功 1:连续采样失败
    * @note   读取流程：
    *         1) 调用 DHT11_Read_Data_Raw() 收集最多3组有效样本；
    *         2) 样本足够时做3点中值滤波，抑制偶发抖动；
    *         3) 样本不足时自动降级为单样本/双样本平均，保证基本可用。
    */
uint8_t DHT11_Read_Data(DHT11_Data_TypeDef *DHT11_Data) 
{
    DHT11_Data_TypeDef sample[DHT11_FILTER_SAMPLES];
    uint8_t ok_cnt;
    uint8_t try_cnt;

    if(DHT11_Data == 0)
        return 1;

    ok_cnt = 0;
    for(try_cnt = 0; (try_cnt < 5) && (ok_cnt < DHT11_FILTER_SAMPLES); try_cnt++)
    {
        if(DHT11_Read_Data_Raw(&sample[ok_cnt]) == 0)
        {
            ok_cnt++;
        }
        Delay_ms(2);
    }

    if(ok_cnt == 0)
        return 1;
    if(ok_cnt == 1)
    {
        DHT11_Data->temp = sample[0].temp;
        DHT11_Data->hum = sample[0].hum;
        return 0;
    }
    if(ok_cnt == 2)
    {
        DHT11_Data->temp = (uint8_t)((sample[0].temp + sample[1].temp) / 2);
        DHT11_Data->hum = (uint8_t)((sample[0].hum + sample[1].hum) / 2);
        return 0;
    }

    DHT11_Data->temp = DHT11_Median3(sample[0].temp, sample[1].temp, sample[2].temp);
    DHT11_Data->hum = DHT11_Median3(sample[0].hum, sample[1].hum, sample[2].hum);
    return 0;
}
/**
    * @brief  DHT11 GPIO初始化（默认输出高电平）
    */
void DHT11_Init(void)
{

    RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK,ENABLE);

    //DHT11 初始化 (默认高电平) 
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_Init(DHT11_GPIO, &GPIO_InitStructure);
    GPIO_SetBits(DHT11_GPIO, DHT11_PIN);
}

/**
    * @brief  DHT11数据引脚切换为输出模式
    */
void DHT11_Mode_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_Init(DHT11_GPIO, &GPIO_InitStructure);
}

/**
    * @brief  DHT11数据引脚切换为输入模式（上拉）
    */
void DHT11_Mode_In(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_Init(DHT11_GPIO, &GPIO_InitStructure);
}
