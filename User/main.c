#include "stm32f10x.h"
#include "Delay.h"
#include "AT24C02.h"
#include "log.h"
#include "iwdg.h"
#include "task_manager.h"

static void System_BootInit(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	delay_init();
	LOG_Init(115200);
	AT24C_Init();
}

int main(void)
{
	System_BootInit();
	printf("\r\n[BOOT] FreeRTOS env monitor start\r\n");

	TaskManager_StartScheduler();
	printf("[BOOT] Scheduler exited unexpectedly\r\n");
	IWDG_Init(IWDG_Prescaler_64, 625);

	while (1)
	{
		IWDG_Feed();
		delay_ms(200);
	}
}
