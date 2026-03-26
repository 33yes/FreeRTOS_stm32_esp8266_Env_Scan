#ifndef __USART2_H
#define __USART2_H

#include "stdio.h"      
#include "stdarg.h"		 
#include "string.h"    

#define USART2_TXBUFF_SIZE   512   		   //定义串口2 发送缓冲区大小（RAM优化）
#define USART2_RXBUFF_SIZE   512               //定义串口2 接收缓冲区大小（RAM优化）

// USART2: WiFi串口，与ESP8266通信
#define WIFI_USART            USART2
#define WIFI_GPIO             GPIOA
#define WIFI_USART_CLK        RCC_APB1Periph_USART2
#define WIFI_GPIO_CLK         RCC_APB2Periph_GPIOA
#define WIFI_TX_PIN           GPIO_Pin_2
#define WIFI_RX_PIN           GPIO_Pin_3

// USART2接收帧超时定时器（TIM4）配置：约50ms超时
#define USART2_RX_TIM                TIM4
#define USART2_RX_TIM_CLK            RCC_APB1Periph_TIM4
#define USART2_RX_TIM_IRQn           TIM4_IRQn
#define USART2_RX_TIMEOUT_ARR        500
#define USART2_RX_TIMEOUT_PSC        7200

/* 兼容部分编辑器/索引器未注入芯片型号宏时 IRQn 未定义的问题 */
#ifndef USART2_IRQn
#define USART2_IRQn                  ((IRQn_Type)38)
#endif

#ifndef TIM4_IRQn
#define TIM4_IRQn                    ((IRQn_Type)30)
#endif

extern unsigned int Usart2_RxCounter;          //外部声明，其他文件可以调用该变量
extern char Usart2_RxBuff[USART2_RXBUFF_SIZE]; //外部声明，其他文件可以调用该变量

/* 初始化串口2（与ESP8266通信） */
void usart2_init(unsigned int);       
/* 串口2格式化输出 */
void u2_printf(char*,...) ;          
/* 发送MQTT封包缓冲区数据 */
void u2_TxData(unsigned char *data);
/* 初始化串口2接收超时定时器（用于分帧） */
void usart2_rx_timer_init(unsigned short int arr, unsigned short int psc);

#endif

