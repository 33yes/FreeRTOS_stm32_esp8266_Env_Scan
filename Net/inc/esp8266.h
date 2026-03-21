
#ifndef __WIFI_H
#define __WIFI_H

#include "usart2.h"	   	

#define RESET_IO(x)    GPIO_WriteBit(GPIOA, GPIO_Pin_4, (BitAction)x)  //PA4控制WiFi的复位

#define WiFi_printf       u2_printf           //串口2控制 WiFi
#define WiFi_RxCounter    Usart2_RxCounter    //串口2控制 WiFi
#define WiFi_RX_BUF       Usart2_RxBuff       //串口2控制 WiFi
#define WiFi_RXBUFF_SIZE  USART2_RXBUFF_SIZE  //串口2控制 WiFi

extern const char SSID[];                     //路由器名称，在main.c中设置
extern const char PASS[];                     //路由器密码，在main.c中设置

extern int ServerPort;
extern char ServerIP[128];                    //存放服务器IP或是域名

/* 初始化ESP8266硬件复位引脚 */
void wifi_reset_io_init(void);
/* 发送AT指令并等待"OK"，0成功，非0失败 */
char WiFi_SendCmd(char *cmd, int timeout);
/* 复位ESP8266并等待ready，0成功，非0失败 */
char WiFi_Reset(int timeout);
/* 连接路由器，0成功，非0失败 */
char WiFi_JoinAP(int timeout);
/* 连接TCP服务器并进入透传，0成功，非0失败 */
char WiFi_Connect_Server(int timeout);
/* SmartConfig配网，0成功，非0失败 */
char WiFi_Smartconfig(int timeout);
/* 等待模块拿到IP，0成功，非0失败 */
char WiFi_WaitAP(int timeout);
/* 一键完成IoT连接流程，0成功，非0失败 */
char WiFi_Connect_IoTServer(void);


#endif


