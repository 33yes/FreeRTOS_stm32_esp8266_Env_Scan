#include "stm32f10x.h" 
#include "usart2.h"     

unsigned int Usart2_RxCounter = 0;      //定义一个变量，记录串口2总共接收了多少字节的数据
char Usart2_RxBuff[USART2_RXBUFF_SIZE]; //定义一个数组，用于保存串口2接收到的数据

/*-------------------------------------------------*/
/*函数名：USART2接收帧超时定时器初始化（TIM4）      */
/*参  数：arr：自动重装值   0~65535                */
/*参  数：psc：时钟预分频数 0~65535                */
/*返回值：无                                       */
/*说  明：定时时间：arr*psc*1000/72000000  单位ms  */
/*-------------------------------------------------*/
void usart2_rx_timer_init(unsigned short int arr, unsigned short int psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(USART2_RX_TIM_CLK, ENABLE);

	TIM_TimeBaseInitStructure.TIM_Period = arr;
	TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(USART2_RX_TIM, &TIM_TimeBaseInitStructure);

	TIM_ClearITPendingBit(USART2_RX_TIM, TIM_IT_Update);
	TIM_ITConfig(USART2_RX_TIM, TIM_IT_Update, ENABLE);
	TIM_Cmd(USART2_RX_TIM, DISABLE);

	NVIC_InitStructure.NVIC_IRQChannel = USART2_RX_TIM_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/*-------------------------------------------------*/
/*函数名：初始化USART2                      */
/*参  数：bound：波特率                            */
/*返回值：无                                       */
/*-------------------------------------------------*/
void usart2_init(unsigned int bound)
{
	//初始化GPIO，PA2-TX, PA3-RX
	GPIO_InitTypeDef GPIO_InitStructure;
	// PA2
	RCC_APB2PeriphClockCmd(WIFI_GPIO_CLK, ENABLE);          //使能GPIOA时钟
	GPIO_InitStructure.GPIO_Pin = WIFI_TX_PIN;              //准备设置PA2
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;      //IO速率50M
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;         //复用推挽输出，用于串口2的发送
	// PA3
	GPIO_InitStructure.GPIO_Pin = WIFI_RX_PIN;              //准备设置PA3
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;   //浮空输入，用于串口2的接收
	GPIO_Init(WIFI_GPIO, &GPIO_InitStructure);

	RCC_APB1PeriphClockCmd(WIFI_USART_CLK, ENABLE); //使能串口2时钟
	USART_DeInit(WIFI_USART);                       //串口2寄存器重新设置为默认值
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = bound;                                    //波特率设置
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;                    //8个数据位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;                         //1个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;                            //无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;                  //收发模式
	USART_Init(WIFI_USART, &USART_InitStructure);

	USART_ClearFlag(WIFI_USART, USART_FLAG_RXNE);              //清除接收标志位
	USART_ITConfig(WIFI_USART, USART_IT_RXNE, ENABLE);          //开启接收中断

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;          //设置串口2中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; //抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         //子优先级0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;            //中断通道使能
	NVIC_Init(&NVIC_InitStructure);                            //设置串口2中断

	USART_Cmd(WIFI_USART, ENABLE);                          //使能串口2
	usart2_rx_timer_init(USART2_RX_TIMEOUT_ARR, USART2_RX_TIMEOUT_PSC);
}

/*-------------------------------------------------*/
/*函数名：串口2 printf函数                         */
/*参  数：char* fmt,...  格式化输出字符串和参数    */
/*返回值：无                                       */
/*-------------------------------------------------*/

char USART2_TxBuff[USART2_TXBUFF_SIZE];

void u2_printf(char* fmt, ...)
{
	unsigned int i, length;

	va_list ap;
	va_start(ap, fmt);
	vsprintf(USART2_TxBuff, fmt, ap);
	va_end(ap);

	length = strlen((const char*)USART2_TxBuff);
	while ((USART2->SR & 0X40) == 0);
	for(i = 0; i < length; i++)
	{
		USART2->DR = USART2_TxBuff[i];
		while ((USART2->SR & 0X40) == 0);
	}
}

/*-------------------------------------------------*/
/*函数名：串口2发送缓冲区中的数据                  */
/*参  数：data：数据                               */
/*返回值：无                                       */
/*-------------------------------------------------*/
void u2_TxData(unsigned char *data)
{
	int i;
	while ((USART2->SR & 0X40) == 0);
	for(i = 1; i <= (data[0] * 256 + data[1]); i++)
	{
		USART2->DR = data[i + 1];
		while ((USART2->SR & 0X40) == 0);
	}
}
