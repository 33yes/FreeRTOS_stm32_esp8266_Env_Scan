/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "usart2.h"
#include "mqtt.h"
#include "string.h"

/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
//void SVC_Handler(void)
//{
//}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
//void PendSV_Handler(void)
//{
//}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
//void SysTick_Handler(void)
//{
//}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
  * @brief  USART2 Interrupt Handler - ESP8266 接收中断处理
  * @note   每当 USART2 接收到字符时触发，数据存入 Usart2_RxBuff
  */
void USART2_IRQHandler(void)
{
	if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
	{
		uint8_t rx_data = (uint8_t)USART_ReceiveData(USART2);

		if (Usart2_RxCounter < (USART2_RXBUFF_SIZE - 1))
		{
			Usart2_RxBuff[Usart2_RxCounter++] = (char)rx_data;
			Usart2_RxBuff[Usart2_RxCounter] = '\0';

      if (Usart2_RxCounter == 1)
      {
        TIM_Cmd(USART2_RX_TIM, ENABLE);
      }
      else
      {
        TIM_SetCounter(USART2_RX_TIM, 0);
      }
		}
		else
		{
			Usart2_RxCounter = 0;
			Usart2_RxBuff[Usart2_RxCounter] = (char)rx_data;
			Usart2_RxCounter = 1;
			Usart2_RxBuff[Usart2_RxCounter] = '\0';
      TIM_SetCounter(USART2_RX_TIM, 0);
      TIM_Cmd(USART2_RX_TIM, ENABLE);
		}

		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}

/**
  * @brief  TIM4中断：USART2接收帧超时处理
  * @note   在设定超时时间内无新字节到来，则认为一帧接收完成
  */
void TIM4_IRQHandler(void)
{
  if (TIM_GetITStatus(USART2_RX_TIM, TIM_IT_Update) != RESET)
  {
    if ((Usart2_RxCounter > 0) && (MQTT_RxDataInPtr != 0))
    {
      uint16_t copy_len = (uint16_t)Usart2_RxCounter;

      if (copy_len > (RBUFF_UNIT - 2))
      {
        copy_len = RBUFF_UNIT - 2;
      }

      memcpy(&MQTT_RxDataInPtr[2], Usart2_RxBuff, copy_len);
      MQTT_RxDataInPtr[0] = (unsigned char)(copy_len / 256);
      MQTT_RxDataInPtr[1] = (unsigned char)(copy_len % 256);

      MQTT_RxDataInPtr += RBUFF_UNIT;
      if (MQTT_RxDataInPtr == MQTT_RxDataEndPtr)
      {
        MQTT_RxDataInPtr = MQTT_RxDataBuf[0];
      }
    }

    Usart2_RxCounter = 0;
    Usart2_RxBuff[0] = '\0';

    TIM_Cmd(USART2_RX_TIM, DISABLE);
    TIM_SetCounter(USART2_RX_TIM, 0);
    TIM_ClearITPendingBit(USART2_RX_TIM, TIM_IT_Update);
  }
}

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
