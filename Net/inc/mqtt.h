
#ifndef __MQTT_H
#define __MQTT_H

#include "stdint.h"

#define  R_NUM               5     //接收缓冲区个数
#define  RBUFF_UNIT          300   //接收缓冲区长度

#define  T_NUM               5     //发送缓冲区个数  
#define  TBUFF_UNIT          300   //发送缓冲区长度

#define  C_NUM               5     //命令缓冲区个数
#define  CBUFF_UNIT          300   //命令缓冲区长度

#define  MQTT_TxData(x)       u2_TxData(x)  //串口2负责数据发送

extern const char PRODUCTID[];      //产品ID，在main.c中定义
extern const char DEVICEID [];      //设备ID，在main.c中定义 
extern const char AUTHENTICATION[]; //鉴权信息，在main.c中定义 
extern const char DATA_TOPIC_NAME[];//topic，Onenet数据点上传topic，在main.c中定义
extern const char SERVER_IP[];		//存放服务器IP或域名，在main.c中定义
extern const int  SERVER_PORT;		//存放服务器的端口号，在main.c中定义

extern unsigned char  MQTT_RxDataBuf[R_NUM][RBUFF_UNIT];//外部变量声明，数据的接收缓冲区,所有服务器发来的数据，存放在该缓冲区,缓冲区第一个字节存放数据长度
extern unsigned char *MQTT_RxDataInPtr;                 //外部变量声明，指向缓冲区存放数据的位置
extern unsigned char *MQTT_RxDataOutPtr;                //外部变量声明，指向缓冲区读取数据的位置
extern unsigned char *MQTT_RxDataEndPtr;                //外部变量声明，指向缓冲区结束的位置
extern unsigned char  MQTT_TxDataBuf[T_NUM][TBUFF_UNIT];//外部变量声明，数据的发送缓冲区,所有发往服务器的数据，存放在该缓冲区,缓冲区第一个字节存放数据长度
extern unsigned char *MQTT_TxDataInPtr;                 //外部变量声明，指向缓冲区存放数据的位置
extern unsigned char *MQTT_TxDataOutPtr;                //外部变量声明，指向缓冲区读取数据的位置
extern unsigned char *MQTT_TxDataEndPtr;                //外部变量声明，指向缓冲区结束的位置
extern unsigned char  MQTT_CMDBuf[C_NUM][CBUFF_UNIT];   //外部变量声明，命令数据的接收缓冲区
extern unsigned char *MQTT_CMDInPtr;                    //外部变量声明，指向缓冲区存放数据的位置
extern unsigned char *MQTT_CMDOutPtr;                   //外部变量声明，指向缓冲区读取数据的位置
extern unsigned char *MQTT_CMDEndPtr;                   //外部变量声明，指向缓冲区结束的位置

extern char ClientID[128];     //外部变量声明，存放客户端ID的缓冲区
extern int  ClientID_len;      //外部变量声明，存放客户端ID的长度
extern char Username[128];     //外部变量声明，存放用户名的缓冲区
extern int  Username_len;	   //外部变量声明，存放用户名的长度
extern char Passward[128];     //外部变量声明，存放密码的缓冲区
extern int  Passward_len;	   //外部变量声明，存放密码的长度
extern char ServerIP[128];     //外部变量声明，存放服务器IP或是域名
extern int  ServerPort;        //外部变量声明，存放服务器的端口号

extern char pingFlag;          //外部变量声明，ping报文状态      0：正常状态，等待计时时间到，发送Ping报文

typedef struct
{
	int16_t temperature_threshold;
	int16_t humidity_threshold;
	uint16_t light_threshold;
} MQTT_Threshold_t;

extern MQTT_Threshold_t g_mqtt_threshold;

/* 初始化MQTT收发/命令缓冲区并预装CONNECT报文 */
void MQTT_Buff_Init(void);
/* 初始化云连接参数（ClientID/Username/Password等） */
void IoT_parameter_init(void);
/* 组装CONNECT报文并写入发送缓冲区 */
void MQTT_ConectPack(void);
/* 组装SUBSCRIBE报文并写入发送缓冲区 */
void MQTT_Subscribe(char *, int);
/* 组装PINGREQ心跳报文并写入发送缓冲区 */
void MQTT_PingREQ(void);
/* 组装QoS0发布报文并写入发送缓冲区 */
void MQTT_PublishQs0(const char *, char *, int);
/* 解析服务器下发的QoS0推送并分发到命令缓冲区 */
void MQTT_DealPushdata_Qs0(unsigned char *);	
/* 发送缓冲区入队 */
void TxDataBuf_Deal(unsigned char *, int);
/* 命令缓冲区入队 */
void CMDBuf_Deal(unsigned char *, int);
/* 阈值初始化（优先EEPROM，失败则默认值） */
void MQTT_ThresholdInit(void);
/* 从AT24C02加载阈值配置 */
uint8_t MQTT_LoadThresholdFromAT24C02(void);
/* 保存阈值配置到AT24C02 */
uint8_t MQTT_SaveThresholdToAT24C02(void);
/* 解析云端阈值JSON并更新本地阈值 */
uint8_t MQTT_ParseAliyunThresholdJson(const char *json_payload);
/* 处理一条云端阈值命令 */
uint8_t MQTT_ProcessAliyunThresholdCommand(void);

#endif
