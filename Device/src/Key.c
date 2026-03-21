#include "stm32f10x.h"                  // Device header
#include "Key.h"

/**
 * @defgroup KEY_CONFIG 按键配置参数
 * @{
 */
#define KEY_DEBOUNCE_COUNT    2U        // 按键消抖计数阈值（需配合扫描周期：计数=阈值*扫描周期
                                        // 例：扫描周期5ms，阈值2 → 消抖时间10ms，可根据硬件调整）
/** @} */

/**
 * @struct  KeyFilter_t
 * @brief   按键消抖过滤结构体（每个按键独立维护一套状态，避免按键间干扰）
 * @note    静态变量存储，仅在本文件内有效
 */
typedef struct
{
    uint8_t stable_state;               // 按键稳定状态（1:未按下(默认，上拉)，0:按下）
    uint8_t debounce_count;             // 消抖计数（连续检测到不稳定状态时累加，达到阈值则更新稳定状态）
} KeyFilter_t;

/**
 * @brief   单个按键扫描+消抖处理函数（内部私有函数，仅本文件调用）
 * @param   gpio: 按键所在GPIO端口（如GPIOB）
 * @param   pin: 按键所在GPIO引脚（如GPIO_Pin_0）
 * @param   key_code: 该按键对应的键码（如KEY_1）
 * @param   filter: 指向该按键的消抖过滤结构体（存储按键状态和计数）
 * @retval  按键扫描结果：
 *          - key_code: 按键有效按下（消抖完成后检测到下降沿）
 *          - KEY_NONE: 无按键按下/消抖中/按键释放
 * @note    1. 仅检测按键“按下”事件（下降沿），不处理释放和长按；
 *          2. 依赖外部周期性调用（建议1~10ms），内部无延时，非阻塞；
 *          3. 按键硬件需配置为上拉输入，默认状态为高电平（1），按下为低电平（0）。
 */
static uint8_t Key_ScanOne(GPIO_TypeDef *gpio, uint16_t pin, uint8_t key_code, KeyFilter_t *filter)
{
    // 读取按键原始电平状态（1:高电平/未按下，0:低电平/按下）
    uint8_t raw_state = (uint8_t)GPIO_ReadInputDataBit(gpio, pin);

    // 检测到原始状态与稳定状态不一致（按键可能抖动/真实按下）
    if (raw_state != filter->stable_state)
    {
        // 消抖计数未达到阈值，累加计数（继续消抖）
        if (filter->debounce_count < KEY_DEBOUNCE_COUNT)
        {
            filter->debounce_count++;
        }
        // 消抖计数达到阈值（连续检测到不稳定状态，判定为状态有效变化）
        if (filter->debounce_count >= KEY_DEBOUNCE_COUNT)
        {
            filter->stable_state = raw_state;  // 更新稳定状态为当前原始状态
            filter->debounce_count = 0U;       // 重置消抖计数，准备下次检测

            // 稳定状态为0（按键有效按下），返回对应键码
            if (filter->stable_state == 0U)
            {
                return key_code;
            }
        }
    }
    else
    {
        // 原始状态与稳定状态一致（无抖动/状态稳定），重置消抖计数
        filter->debounce_count = 0U;
    }

    // 无有效按键按下（或消抖中/按键释放），返回KEY_NONE
    return KEY_NONE;
}

/*按键初始化*/
void Key_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(KEY_GPIO, ENABLE);		//开启GPIOB的时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = KEY1_PIN | KEY2_PIN | KEY3_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(KEY_GPIO, &GPIO_InitStructure);
}

/**
  * @brief  非阻塞式按键扫描（纯轮询消抖）
    * @return uint8_t 键码 (0:无按下, 1:KEY1光标上移, 2:KEY2确认, 3:KEY3光标下移)
  * @note   1. 此函数内部无任何延时和RTOS依赖；
  *         2. 需周期性轮询调用，建议周期1~10ms；
  *         3. 仅检测“按下”事件（下降沿），不检测释放和长按。
  */
uint8_t Key_GetNum(void)
{
    static KeyFilter_t key1_filter = {1U, 0U};
    static KeyFilter_t key2_filter = {1U, 0U};
    static KeyFilter_t key3_filter = {1U, 0U};

    uint8_t key_num;

    key_num = Key_ScanOne(KEY_GPIO, KEY1_PIN, KEY_1, &key1_filter);
    if (key_num != KEY_NONE)
    {
        return key_num;
    }

    key_num = Key_ScanOne(KEY_GPIO, KEY2_PIN, KEY_2, &key2_filter);
    if (key_num != KEY_NONE)
    {
        return key_num;
    }

    key_num = Key_ScanOne(KEY_GPIO, KEY3_PIN, KEY_3, &key3_filter);
    return key_num;
}
