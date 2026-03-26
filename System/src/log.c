#include "log.h"

#if defined(__CC_ARM)
#pragma import(__use_no_semihosting_swi)
struct __FILE { int handle; };
FILE __stdout;
FILE __stdin;

int ferror(FILE *f)
{
	(void)f;
	return EOF;
}

void _sys_exit(int x)
{
	(void)x;
	while (1)
	{
	}
}
#endif

/**
 * @brief  初始化日志串口(USART1)
 * @param  baudrate: 波特率，例如 115200
 * @note   默认引脚：PA9(TX)、PA10(RX)
 */
void LOG_Init(uint32_t baudrate)
{

	/* 开启 USART1 与 GPIOA 时钟 */
	RCC_APB2PeriphClockCmd(LOG_USART_CLK | LOG_GPIO_CLK, ENABLE);

	/* TX: 复用推挽输出 */
    GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = LOG_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LOG_GPIO, &GPIO_InitStructure);

	/* RX: 浮空输入 */
	GPIO_InitStructure.GPIO_Pin = LOG_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(LOG_GPIO, &GPIO_InitStructure);

	/* 串口参数：8N1，收发使能 */
    USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(LOG_USART, &USART_InitStructure);

	USART_Cmd(LOG_USART, ENABLE);
}

/**
 * @brief  输出单个字符到日志串口
 */
void LOG_WriteChar(char ch)
{
	/* 等待发送数据寄存器空 */
	while (USART_GetFlagStatus(LOG_USART, USART_FLAG_TXE) == RESET)
	{
	}
	USART_SendData(LOG_USART, (uint16_t)ch);
}

/**
 * @brief  输出字符串到日志串口
 * @param  str: 以 '\0' 结尾的字符串
 */
void LOG_WriteString(const char *str)
{
	if (str == 0)
	{
		return;
	}

	while (*str != '\0')
	{
		LOG_WriteChar(*str);
		str++;
	}
}

/**
 * @brief  printf 底层重定向接口
 * @note   调用 LOG_Init() 后，printf 默认经 USART1 输出。
 */
int fputc(int ch, FILE *f)
{
	(void)f;
	LOG_WriteChar((char)ch);
	return ch;
}
