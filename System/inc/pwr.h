#ifndef __PWR_H
#define __PWR_H

#include "stm32f10x.h"

void PWR_Init(void);
void PWR_EnterSleepMode(void);
void PWR_EnterStopMode(void);
void PWR_ReConfigSystemClock(void);

#endif