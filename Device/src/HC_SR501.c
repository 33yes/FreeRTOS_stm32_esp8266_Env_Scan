#include "HC_SR501.h"

/*---------------------------------------------------------------*/
/* 文件说明：HC_SR501.c                                          */
/* 功  能：HC-SR501人体红外模块基础驱动                          */
/* 说  明：                                                       */
/*  1. HC_SR501_Init()：配置PA11为输入上拉                        */
/*  2. HC_SR501_ReadRaw()：读取瞬时状态（0无人/1有人）            */
/*  3. HC_SR501_ReadStable()：简单多数投票，抑制抖动              */
/*---------------------------------------------------------------*/

void HC_SR501_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(PIR_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Pin = PIR_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(PIR_PORT, &GPIO_InitStructure);
}

PIR_State_t HC_SR501_ReadRaw(void)
{
	if(GPIO_ReadInputDataBit(PIR_PORT, PIR_PIN) == Bit_SET)
	{
		return PIR_MOTION;
	}

	return PIR_NO_MOTION;
}

PIR_State_t HC_SR501_ReadStable(uint8_t sampleCount)
{
	uint8_t i;
	uint8_t highCount;

	if(sampleCount == 0)
	{
		sampleCount = 1;
	}

	highCount = 0;
	for(i = 0; i < sampleCount; i++)
	{
		if(HC_SR501_ReadRaw() == PIR_MOTION)
		{
			highCount++;
		}
	}

	if(highCount >= ((sampleCount / 2) + 1))
	{
		return PIR_MOTION;
	}

	return PIR_NO_MOTION;
}
