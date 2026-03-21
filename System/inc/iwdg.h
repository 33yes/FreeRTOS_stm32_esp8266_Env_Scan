#ifndef __IWDG_H
#define __IWDG_H

#include "stm32f10x.h"

/**
 * @brief  初始化独立看门狗
 * @param  prer: 分频因子，建议使用 IWDG_Prescaler_xx 宏
 * @param  rlr: 重装载寄存器值 (0~4095)
 * @note   溢出时间计算: Tout = (4 * 2^prer * rlr) / 40 (单位: ms)
 * @note   调用后看门狗立即启动且不可关闭，必须周期性调用 IWDG_Feed()
 *         建议喂狗周期 < 计算超时时间的 1/2。
 */
void IWDG_Init(uint8_t prer, uint16_t rlr);

/**
 * @brief  喂狗（重置计数器）
 * @note   推荐在系统节拍任务/健康监测任务中统一调用。
 */
void IWDG_Feed(void);

#endif
