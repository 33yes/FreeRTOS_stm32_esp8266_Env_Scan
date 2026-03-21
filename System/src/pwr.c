#include "pwr.h"

/**
 * @brief  电源管理初始化
 */
void PWR_Init(void)
{
    // 开启电源管理外设时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
}

/**
 * @brief  进入睡眠模式 (Sleep Mode)
 * @note   CPU 停止，但外设依然工作。
 * 由于 FreeRTOS 的系统滴答定时器（SysTick）一直在运行，
 * 所以 CPU 会每隔 1ms (根据你的 configTICK_RATE_HZ 定) 被唤醒一次。
 * 这确保了后台任务（如传感器轮询）不会死掉。
 */
void PWR_EnterSleepMode(void)
{
    SCB->SCR &= (uint32_t)(~SCB_SCR_SLEEPDEEP_Msk);
    __WFI(); 
}

/**
 * @brief  进入停止模式 (Stop Mode)
 * @note   唤醒后系统时钟会退回到HSI，需调用 PWR_ReConfigSystemClock() 恢复PLL。
 */
void PWR_EnterStopMode(void)
{
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    PWR_ClearFlag(PWR_FLAG_WU);
    PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFI);
    SCB->SCR &= (uint32_t)(~SCB_SCR_SLEEPDEEP_Msk);
}

/**
 * @brief  STOP唤醒后恢复系统时钟到72MHz(外部晶振+PLL)
 */
void PWR_ReConfigSystemClock(void)
{
    ErrorStatus hse_startup;

    RCC_HSEConfig(RCC_HSE_ON);
    hse_startup = RCC_WaitForHSEStartUp();

    if (hse_startup == SUCCESS)
    {
        FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
        FLASH_SetLatency(FLASH_Latency_2);

        RCC_HCLKConfig(RCC_SYSCLK_Div1);
        RCC_PCLK2Config(RCC_HCLK_Div1);
        RCC_PCLK1Config(RCC_HCLK_Div2);

        RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
        RCC_PLLCmd(ENABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        {
        }

        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        while (RCC_GetSYSCLKSource() != 0x08)
        {
        }
    }
}
