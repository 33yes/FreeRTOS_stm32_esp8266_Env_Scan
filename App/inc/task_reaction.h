#ifndef __TASK_REACTION_H
#define __TASK_REACTION_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "motor.h"
#include "humidity.h"

/* 执行器控制模式：自动/手动 */
typedef enum
{
	CTRL_MODE_AUTO = 0,
	CTRL_MODE_MANUAL = 1
} ControlMode_t;

/* 执行器运行状态快照（供OLED显示和云端上报使用） */
typedef struct
{
	ControlMode_t light_mode;
	ControlMode_t motor_mode;
	ControlMode_t humidifier_mode;

	uint8_t light_level;                /* 0:off 1:level1 2:level2 */
	Motor_Speed_e motor_level;          /* off/level1/level2 */
	Humidifier_State_e humidifier_on;   /* off/on */
} ReactionStatus_t;

/* 初始化执行器任务依赖的底层驱动和状态 */
void TaskReaction_Init(void);
/* 执行器主任务：自动控制刷新+输出下发 */
void Task_Reaction(void *pvParameters);

/* 手动/自动控制接口（供菜单任务调用） */
void TaskReaction_SetLightAuto(void);
void TaskReaction_SetLightManual(uint8_t level);
void TaskReaction_SetMotorAuto(void);
void TaskReaction_SetMotorManual(Motor_Speed_e level);
void TaskReaction_SetHumidifierAuto(void);
void TaskReaction_SetHumidifierManual(Humidifier_State_e state);

/* 读取当前执行器状态 */
BaseType_t TaskReaction_GetStatus(ReactionStatus_t *outStatus);

#endif
