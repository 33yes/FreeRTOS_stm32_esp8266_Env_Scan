#include "stm32f10x.h"                  // Device header
#include "iwdg.h"

/**
 * @brief  初始化 IWDG
 * @param  prer: 分频因子 (4 对应 64 分频)
 * @param  rlr: 重装载值 (决定溢出时间)
 * @details 
 * 若 LSI = 40kHz, prer = 4 (64分频), rlr = 625:
 * Tout = (64 * 625) / 40 = 1000ms (1秒后不喂狗即重启)
 */
void IWDG_Init(uint8_t prer, uint16_t rlr)
{
    if (prer > IWDG_Prescaler_256)
    {
        prer = IWDG_Prescaler_64;
    }

    if (rlr > 0x0FFF)
    {
        rlr = 0x0FFF;
    }

    // 独立看门狗写使能
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    
    // 设置预分频为64
    IWDG_SetPrescaler(prer);
    
    // 设置重装载值为625
    IWDG_SetReload(rlr);
    
    // 重载计数器值（先喂一次狗）
    IWDG_ReloadCounter();
    
    // 使能看门狗
    IWDG_Enable();
}

/**
 * @brief  喂狗
 */
void IWDG_Feed(void)
{
    IWDG_ReloadCounter();
}