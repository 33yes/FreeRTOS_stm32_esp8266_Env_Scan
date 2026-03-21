#include "stm32f10x.h"
#include "motor.h"

/* ======================== 全局状态变量 ======================== */
static Motor_Speed_e g_Motor_State = MOTOR_STOP;   /*!< 电机当前转速状态 */


/**
  * @brief  PWMA 初始化
  * @note   使用 Timer2 Channel 1 (PA0)，频率 20kHz
  */
static void PWMA_Init(void)
{
    /* 1. 开启时钟 */
    RCC_APB1PeriphClockCmd(MOTOR_PWMA_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(MOTOR_PWMA_GPIO_CLK, ENABLE);
    
    /* 2. GPIO 配置为复用推挽输出 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = MOTOR_PWMA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_PWMA_GPIO, &GPIO_InitStructure);
    
    /* 3. 时基单元配置 */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;     /*!< ARR=99，PWM 周期对应 0-100 占空比 */
    TIM_TimeBaseInitStructure.TIM_Prescaler = 36 - 1;   /*!< PSC=35，72MHz/(35+1)/(99+1)=20kHz */
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(MOTOR_PWMA_TIM, &TIM_TimeBaseInitStructure);
    
    /* 4. PWM 模式配置 */
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0; /*!< 初始占空比 0 */
    TIM_OC1Init(MOTOR_PWMA_TIM, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(MOTOR_PWMA_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(MOTOR_PWMA_TIM, ENABLE);
    
    /* 5. 启动定时器 */
    TIM_Cmd(MOTOR_PWMA_TIM, ENABLE);
}

/**
  * @brief  初始化电机方向控制引脚 (AIN1/AIN2)
  */
static void Motor_GPIO_Init(void)
{
    /* AIN1 和 AIN2初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(MOTOR_GPIO_CLK, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = MOTOR_AIN1_PIN | MOTOR_AIN2_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_GPIO, &GPIO_InitStructure);
    
    /* 初始状态：停止 */
    GPIO_ResetBits(MOTOR_GPIO, MOTOR_AIN1_PIN);
    GPIO_ResetBits(MOTOR_GPIO, MOTOR_AIN2_PIN);
}

/**
  * @brief  电机驱动初始化（方向GPIO + PWM）
  */
void Motor_Init(void)
{
    Motor_GPIO_Init();
    PWMA_Init();
    Motor_Stop();
}



/**
  * @brief  设置电机转速（仅支持正转）
  * @param  Speed: 转速档位 
  *         - MOTOR_STOP   (0)   : 停止
  *         - MOTOR_LEVEL1 (50)  : 一档（50% PWM）
  *         - MOTOR_LEVEL2 (100) : 二档（100% PWM）
  */
void Motor_SetSpeed(Motor_Speed_e Speed)
{
  uint16_t duty;

  if (Speed == MOTOR_STOP)
    {
    GPIO_ResetBits(MOTOR_GPIO, MOTOR_AIN1_PIN);
    GPIO_ResetBits(MOTOR_GPIO, MOTOR_AIN2_PIN);
    TIM_SetCompare1(MOTOR_PWMA_TIM, 0);
    g_Motor_State = MOTOR_STOP;
    return;
  }

  duty = (uint16_t)Speed;
  if (duty > 99U)
  {
    duty = 99U;
    }

  /* 正转：AIN1=1, AIN2=0 */
  GPIO_SetBits(MOTOR_GPIO, MOTOR_AIN1_PIN);
  GPIO_ResetBits(MOTOR_GPIO, MOTOR_AIN2_PIN);
  TIM_SetCompare1(MOTOR_PWMA_TIM, duty);

  if (duty <= (uint16_t)MOTOR_LEVEL1)
  {
    g_Motor_State = MOTOR_LEVEL1;
  }
  else
  {
    g_Motor_State = MOTOR_LEVEL2;
  }
}

/**
  * @brief  停止电机
  */
void Motor_Stop(void)
{
    Motor_SetSpeed(MOTOR_STOP);
}

/**
  * @brief  获取电机当前转速状态
  * @return Motor_Speed_e 
  */
Motor_Speed_e Motor_GetSpeed(void)
{
  return g_Motor_State;
}

