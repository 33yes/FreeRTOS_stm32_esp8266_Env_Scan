#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"

#ifndef DWT
typedef struct
{
	volatile uint32_t CTRL;
	volatile uint32_t CYCCNT;
} Delay_DWT_Compat_TypeDef;

#define DWT ((Delay_DWT_Compat_TypeDef *)0xE0001000UL)
#endif

#ifndef DWT_CTRL_CYCCNTENA_Msk
#define DWT_CTRL_CYCCNTENA_Msk (1UL << 0)
#endif

static uint32_t s_cycles_per_us = 72U;
static uint32_t s_ms_per_tick = 1U;

static void Delay_DwtInit(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0U;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_init(void)
{
	SystemCoreClockUpdate();
	s_cycles_per_us = SystemCoreClock / 1000000U;
	if (s_cycles_per_us == 0U)
	{
		s_cycles_per_us = 1U;
	}
	s_ms_per_tick = 1000U / configTICK_RATE_HZ;
	if (s_ms_per_tick == 0U)
	{
		s_ms_per_tick = 1U;
	}
	Delay_DwtInit();
}

void delay_us(uint32_t us)
{
	uint32_t start;
	uint32_t ticks;

	if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
	{
		Delay_DwtInit();
	}

	start = DWT->CYCCNT;
	ticks = us * s_cycles_per_us;
	while ((DWT->CYCCNT - start) < ticks)
	{
	}
}

void delay_ms(uint32_t ms)
{
	if ((xTaskGetTickCount() != 0U) && (ms >= s_ms_per_tick))
	{
		vTaskDelay(pdMS_TO_TICKS(ms));
		ms %= s_ms_per_tick;
	}

	while (ms--)
	{
		delay_us(1000U);
	}
}

void delay_s(uint32_t s)
{
	while (s--)
	{
		delay_ms(1000U);
	}
}

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  */
void Delay_us(uint32_t xus)
{
	delay_us(xus);
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	delay_ms(xms);
}
 
/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	delay_s(xs);
} 
