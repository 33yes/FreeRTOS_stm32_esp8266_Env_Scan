#include "stm32f10x.h"    
#include "mqtt.h"         
#include "string.h"       
#include "stdio.h"             
#include "esp8266.h"
#include "AT24C02.h"
#include "stdlib.h"

unsigned char  MQTT_RxDataBuf[R_NUM][RBUFF_UNIT];  //数据的接收缓冲区,所有服务器发来的数据，存放在该缓冲区,缓冲区第一个字节存放数据长度
unsigned char *MQTT_RxDataInPtr;                   //指向接收缓冲区存放数据的位置
unsigned char *MQTT_RxDataOutPtr;                  //指向接收缓冲区读取数据的位置
unsigned char *MQTT_RxDataEndPtr;                  //指向接收缓冲区结束的位置

unsigned char  MQTT_TxDataBuf[T_NUM][TBUFF_UNIT];  //数据的发送缓冲区,所有发往服务器的数据，存放在该缓冲区,缓冲区第一个字节存放数据长度
unsigned char *MQTT_TxDataInPtr;                   //指向发送缓冲区存放数据的位置
unsigned char *MQTT_TxDataOutPtr;                  //指向发送缓冲区读取数据的位置
unsigned char *MQTT_TxDataEndPtr;                  //指向发送缓冲区结束的位置

unsigned char  MQTT_CMDBuf[C_NUM][CBUFF_UNIT];     //命令数据的接收缓冲区
unsigned char *MQTT_CMDInPtr;                      //指向命令缓冲区存放数据的位置
unsigned char *MQTT_CMDOutPtr;                     //指向命令缓冲区读取数据的位置
unsigned char *MQTT_CMDEndPtr;                     //指向命令缓冲区结束的位置

char ClientID[128];                                //存放客户端ID的缓冲区
int  ClientID_len;                                 //存放客户端ID的长度

char Username[128];                                //存放用户名的缓冲区
int  Username_len;								   //存放用户名的长度

char Passward[128];                                //存放密码的缓冲区
int  Passward_len;								   //存放密码的长度

char ServerIP[128];                                //存放服务器IP或是域名
int  ServerPort;                                   //存放服务器的端口号

int   Fixed_len;                       			   //固定报头长度
int   Variable_len;                     		   //可变报头长度
int   Payload_len;                       		   //有效负荷长度
unsigned char  temp_buff[TBUFF_UNIT];			   //临时缓冲区，构建报文用

char pingFlag = 0;       //ping报文状态       0：正常状态，等待计时时间到，发送Ping报文
                         //ping报文状态       1：Ping报文已发送，当收到 服务器回复报文的后 将1置为0

MQTT_Threshold_t g_mqtt_threshold = {30, 80, 300}; //默认阈值：温度30，湿度80，光照300

#define THRESHOLD_EE_MAGIC_ADDR      0x10
#define THRESHOLD_EE_VER_ADDR        0x11
#define THRESHOLD_EE_TEMP_L_ADDR     0x12
#define THRESHOLD_EE_TEMP_H_ADDR     0x13
#define THRESHOLD_EE_HUMI_L_ADDR     0x14
#define THRESHOLD_EE_HUMI_H_ADDR     0x15
#define THRESHOLD_EE_LIGHT_L_ADDR    0x16
#define THRESHOLD_EE_LIGHT_H_ADDR    0x17
#define THRESHOLD_EE_CRC_ADDR        0x18

#define THRESHOLD_EE_MAGIC           0xA5
#define THRESHOLD_EE_VER             0x01

/*----------------------------------------------------------*/
/*函数名：threshold_crc_calc                                */
/*功  能：计算阈值配置在AT24C02中的校验值（异或校验）         */
/*说  明：用于判断EEPROM中的阈值数据是否被破坏                */
/*参  数：无                                                */
/*返回值：校验值（uint8_t）                                  */
/*----------------------------------------------------------*/
static uint8_t threshold_crc_calc(void)
{
	uint8_t crc;

	crc = THRESHOLD_EE_MAGIC ^ THRESHOLD_EE_VER;
	crc ^= AT24C_ReadByte(THRESHOLD_EE_TEMP_L_ADDR);
	crc ^= AT24C_ReadByte(THRESHOLD_EE_TEMP_H_ADDR);
	crc ^= AT24C_ReadByte(THRESHOLD_EE_HUMI_L_ADDR);
	crc ^= AT24C_ReadByte(THRESHOLD_EE_HUMI_H_ADDR);
	crc ^= AT24C_ReadByte(THRESHOLD_EE_LIGHT_L_ADDR);
	crc ^= AT24C_ReadByte(THRESHOLD_EE_LIGHT_H_ADDR);

	return crc;
}

/*----------------------------------------------------------*/
/*函数名：parse_json_int_by_key                             */
/*功  能：从JSON字符串中按键名提取整数值                     */
/*说  明：轻量级解析，不依赖cJSON，适合STM32资源受限场景      */
/*参  数：json_payload：原始JSON字符串                       */
/*参  数：key：要查找的键（例如"\"TempThreshold\""）        */
/*参  数：value：输出参数，返回解析出的整数值                */
/*返回值：1=找到并解析成功，0=未找到或解析失败               */
/*----------------------------------------------------------*/
static uint8_t parse_json_int_by_key(const char *json_payload, const char *key, int *value)
{
	const char *k;
	const char *colon;
	char *endptr;
	long v;

	if((json_payload == 0) || (key == 0) || (value == 0))
		return 0;

	k = strstr(json_payload, key);
	if(k == 0)
		return 0;

	colon = strchr(k, ':');
	if(colon == 0)
		return 0;

	colon++;
	while((*colon == ' ') || (*colon == '\t') || (*colon == '"'))
		colon++;

	v = strtol(colon, &endptr, 10);
	if(endptr == colon)
		return 0;

	*value = (int)v;
	return 1;
}

/*----------------------------------------------------------*/
/*函数名：clamp_int                                         */
/*功  能：将整数限制在指定范围内                             */
/*参  数：v：待处理数值                                     */
/*参  数：min_v：最小值                                     */
/*参  数：max_v：最大值                                     */
/*返回值：范围限制后的结果                                   */
/*----------------------------------------------------------*/
static int clamp_int(int v, int min_v, int max_v)
{
	if(v < min_v)
		return min_v;
	if(v > max_v)
		return max_v;
	return v;
}

/*----------------------------------------------------------*/
/*函数名：mqtt_encode_remaining_length                      */
/*功  能：按MQTT协议编码Remaining Length字段               */
/*参  数：buf：输出缓冲区                                   */
/*参  数：remaining_len：待编码长度                         */
/*返回值：编码后字节数                                      */
/*----------------------------------------------------------*/
static int mqtt_encode_remaining_length(unsigned char *buf, int remaining_len)
{
	int temp;
	int len;

	len = 0;
	do{
		temp = remaining_len % 128;
		remaining_len = remaining_len / 128;
		if(remaining_len > 0)
			temp |= 0x80;
		buf[len] = (unsigned char)temp;
		len++;
	}while(remaining_len > 0);

	return len;
}

/*----------------------------------------------------------*/
/*函数名：mqtt_decode_remaining_length                      */
/*功  能：按MQTT协议解码Remaining Length字段               */
/*参  数：buf：输入缓冲区                                   */
/*参  数：consumed：输出参数，返回占用字节数               */
/*返回值：解码后的长度                                      */
/*----------------------------------------------------------*/
static int mqtt_decode_remaining_length(const unsigned char *buf, int *consumed)
{
	int value;
	int multiplier;
	int temp;
	int len;

	value = 0;
	multiplier = 1;
	len = 0;
	do{
		temp = buf[len];
		value += (temp & 127) * multiplier;
		multiplier *= 128;
		len++;
	}while((temp & 128) != 0);

	if(consumed != 0)
		*consumed = len;

	return value;
}

/*----------------------------------------------------------*/
/*函数名：初始化接收,发送,命令数据的 缓冲区 以及各状态参数  */
/*参  数：无                                                */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void MQTT_Buff_Init(void)
{	
	MQTT_RxDataInPtr=MQTT_RxDataBuf[0]; 				 //指向发送缓冲区存放数据的指针归位
	MQTT_RxDataOutPtr=MQTT_RxDataInPtr; 				 //指向发送缓冲区读取数据的指针归位
	MQTT_RxDataEndPtr=MQTT_RxDataBuf[R_NUM-1];		     //指向发送缓冲区结束的指针归位
	
	MQTT_TxDataInPtr=MQTT_TxDataBuf[0];					 //指向发送缓冲区存放数据的指针归位
	MQTT_TxDataOutPtr=MQTT_TxDataInPtr;				     //指向发送缓冲区读取数据的指针归位
	MQTT_TxDataEndPtr=MQTT_TxDataBuf[T_NUM-1];           //指向发送缓冲区结束的指针归位
	
	MQTT_CMDInPtr=MQTT_CMDBuf[0];                        //指向命令缓冲区存放数据的指针归位
	MQTT_CMDOutPtr=MQTT_CMDInPtr;                        //指向命令缓冲区读取数据的指针归位
	MQTT_CMDEndPtr=MQTT_CMDBuf[C_NUM-1];              	 //指向命令缓冲区结束的指针归位

	MQTT_ConectPack();                                	 //发送缓冲区添加连接报文
	MQTT_ThresholdInit();
	
	pingFlag = 0;   //ping返回值清零
}

/*----------------------------------------------------------*/
/*函数名：云初始化参数，得到客户端ID，用户名和密码          */
/*参  数：无                                                */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void IoT_parameter_init(void)
{	
	memset(ClientID,0,128);                              //客户端ID的缓冲区全部清零
	sprintf(ClientID,"%s",DEVICEID);                     //OneNet：ClientID 使用 DeviceID
	ClientID_len = strlen(ClientID);                     //计算客户端ID的长度
	
	memset(Username,0,128);                              //用户名的缓冲区全部清零
	sprintf(Username,"%s",PRODUCTID);                   //OneNet：Username 使用 ProductID
	Username_len = strlen(Username);                     //计算用户名的长度
	
	memset(Passward,0,128);                              //用户名的缓冲区全部清零
	sprintf(Passward,"%s",AUTHENTICATION);               //构建密码，并存入缓冲区
	Passward_len = strlen(Passward);                     //计算密码的长度
	
	memset(ServerIP,0,128);  
	sprintf(ServerIP,"%s",SERVER_IP);              		//构建服务器域名
	ServerPort = SERVER_PORT;                           //服务器端口号
	
	printf("SERVER: %s:%d\r\n", ServerIP, ServerPort);
	printf("CLIENT_ID: %s\r\n", ClientID);
	printf("USERNAME: %s\r\n", Username);
	printf("PASSWORD: %s\r\n", Passward);
}

/*----------------------------------------------------------*/
/*函数名：连接服务器报文                                    */
/*参  数：无                                                */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void MQTT_ConectPack(void)
{	
	int Remaining_len;
	int topic_offset;
	
	Fixed_len = 1;                                                        //连接报文中，固定报头长度暂时先=1
	Variable_len = 10;                                                    //连接报文中，可变报头长度=10
	Payload_len = 2 + ClientID_len + 2 + Username_len + 2 + Passward_len; //连接报文中，负载长度      
	Remaining_len = Variable_len + Payload_len;                           //剩余长度=可变报头长度+负载长度
	
	temp_buff[0]=0x10;                    //固定报头第1个字节 ：固定0x01		
	Fixed_len += mqtt_encode_remaining_length(&temp_buff[1], Remaining_len);
	
	temp_buff[Fixed_len + 0] = 0x00;      //可变报头第1个字节 ：固定0x00	            
	temp_buff[Fixed_len + 1] = 0x04;      //可变报头第2个字节 ：固定0x04
	temp_buff[Fixed_len + 2] = 0x4D;	  //可变报头第3个字节 ：固定0x4D
	temp_buff[Fixed_len + 3] = 0x51;	  //可变报头第4个字节 ：固定0x51
	temp_buff[Fixed_len + 4] = 0x54;	  //可变报头第5个字节 ：固定0x54
	temp_buff[Fixed_len + 5] = 0x54;      //可变报头第6个字节 ：固定0x54
	temp_buff[Fixed_len + 6] = 0x04;	  //可变报头第7个字节 ：固定0x04
	temp_buff[Fixed_len + 7] = 0xC2;	  //可变报头第8个字节 ：使能用户名和密码校验，不使用遗嘱，不保留会话
	temp_buff[Fixed_len + 8] = 0x00; 	  //可变报头第9个字节 ：保活时间高字节 0x00
	temp_buff[Fixed_len + 9] = 0x64;	  //可变报头第10个字节：保活时间高字节 0x64   100s
	topic_offset = Fixed_len + 10;
	
	/*     CLIENT_ID      */
	temp_buff[topic_offset + 0] = ClientID_len/256;                			  	//客户端ID长度高字节
	temp_buff[topic_offset + 1] = ClientID_len%256;               			  	//客户端ID长度低字节
	memcpy(&temp_buff[topic_offset + 2],ClientID,ClientID_len);                 //复制过来客户端ID字串	
	/*     用户名        */
	temp_buff[topic_offset + 2 + ClientID_len] = Username_len/256; 				//用户名长度高字节
	temp_buff[topic_offset + 3 + ClientID_len] = Username_len%256; 				//用户名长度低字节
	memcpy(&temp_buff[topic_offset + 4 + ClientID_len],Username,Username_len);    //复制过来用户名字串	
	/*      密码        */
	temp_buff[topic_offset + 4 + ClientID_len + Username_len] = Passward_len/256;	//密码长度高字节
	temp_buff[topic_offset + 5 + ClientID_len + Username_len] = Passward_len%256;	//密码长度低字节
	memcpy(&temp_buff[topic_offset + 6 + ClientID_len + Username_len],Passward,Passward_len); //复制过来密码字串

	TxDataBuf_Deal(temp_buff, Fixed_len + Variable_len + Payload_len);      //加入发送数据缓冲区
}

/*----------------------------------------------------------*/
/*函数名：SUBSCRIBE订阅topic报文                            */
/*参  数：QoS：订阅等级                                     */
/*参  数：topic_name：订阅topic报文名称                     */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void MQTT_Subscribe(char *topic_name, int QoS)
{	
	int topic_len;

	topic_len = strlen(topic_name);
	Fixed_len = 2;                              		   //SUBSCRIBE报文中，固定报头长度=2
	Variable_len = 2;                          			   //SUBSCRIBE报文中，可变报头长度=2	
	Payload_len = 2 + topic_len + 1;   		   //计算有效负荷长度 = 2字节(topic_name长度)+ topic_name字符串的长度 + 1字节服务等级
	
	temp_buff[0] = 0x82;                                   //第1个字节 ：固定0x82                      
	temp_buff[1] = Variable_len + Payload_len;             //第2个字节 ：可变报头+有效负荷的长度	
	temp_buff[2] = 0x00;                                   //第3个字节 ：报文标识符高字节，固定使用0x00
	temp_buff[3] = 0x01;		                           //第4个字节 ：报文标识符低字节，固定使用0x01
	temp_buff[4] = topic_len/256;                          //第5个字节 ：topic_name长度高字节
	temp_buff[5] = topic_len%256;		           //第6个字节 ：topic_name长度低字节
	memcpy(&temp_buff[6], topic_name, topic_len);          //第7个字节开始 ：复制过来topic_name字串		
	temp_buff[6 + topic_len] = QoS;                        //最后1个字节：订阅等级
	
	TxDataBuf_Deal(temp_buff, Fixed_len + Variable_len + Payload_len);  //加入发送数据缓冲区
}

/*----------------------------------------------------------*/
/*函数名：PING报文，心跳包                                   */
/*参  数：无                                                */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void MQTT_PingREQ(void)
{
	temp_buff[0] = 0xC0;              //第1个字节 ：固定0xC0                      
	temp_buff[1] = 0x00;              //第2个字节 ：固定0x00 

	TxDataBuf_Deal(temp_buff, 2);     //加入数据到缓冲区
}

/*----------------------------------------------------------*/
/*函数名：等级0 发布消息报文                                */
/*参  数：topic_name：topic名称                             */
/*参  数：data：数据                                        */ 
/*参  数：data_len：数据长度                                */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void MQTT_PublishQs0(const char *topic, char *data, int data_len)
{	
	int Remaining_len;
	int topic_len;
	
	topic_len = strlen(topic);
	Fixed_len = 1;                              //固定报头长度暂时先等于：1字节
	Variable_len = 2 + topic_len;               //可变报头长度：2字节(topic长度)+ topic字符串的长度
	Payload_len = data_len;                     //有效负荷长度：就是data_len
	Remaining_len = Variable_len + Payload_len; //剩余长度=可变报头长度+负载长度
	
	temp_buff[0] = 0x30;                      	//固定报头第1个字节 ：固定0x30   	
	Fixed_len += mqtt_encode_remaining_length(&temp_buff[1], Remaining_len);
		             
	temp_buff[Fixed_len+0] = topic_len/256;                           //可变报头第1个字节     ：topic长度高字节
	temp_buff[Fixed_len+1] = topic_len%256;		                  //可变报头第2个字节     ：topic长度低字节
	memcpy(&temp_buff[Fixed_len+2], topic,topic_len);                 //可变报头第3个字节开始 ：拷贝topic字符串	
	memcpy(&temp_buff[Fixed_len + 2 + topic_len], data, data_len);    //有效负荷：拷贝data数据
	
	TxDataBuf_Deal(temp_buff, Fixed_len + Variable_len + Payload_len);//加入发送数据缓冲区	
}

/*----------------------------------------------------------*/
/*函数名：MQTT_PublishOneNetDpJson                          */
/*功  能：将JSON封装为OneNet $dp格式后发布                  */
/*说  明：OneNet $dp消息体 = 0x03 + 2字节长度 + JSON正文      */
/*参  数：json_payload：JSON正文（不含$dp头）                */
/*参  数：json_len：JSON正文长度                             */
/*返回值：0成功，1失败                                      */
/*----------------------------------------------------------*/
uint8_t MQTT_PublishOneNetDpJson(const char *json_payload, int json_len)
{
	unsigned char dp_frame[TBUFF_UNIT];

	if((json_payload == 0) || (json_len <= 0))
		return 1;

	if(json_len > (TBUFF_UNIT - 3))
		return 1;

	dp_frame[0] = 0x03;
	dp_frame[1] = (unsigned char)((json_len >> 8) & 0xFF);
	dp_frame[2] = (unsigned char)(json_len & 0xFF);
	memcpy(&dp_frame[3], json_payload, json_len);

	MQTT_PublishQs0(DATA_TOPIC_NAME, (char *)dp_frame, json_len + 3);
	return 0;
}

/*----------------------------------------------------------*/
/*函数名：处理服务器发来的等级0的推送                       */
/*参  数：redata：接收的数据                                */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void MQTT_DealPushdata_Qs0(unsigned char *redata)
{
	int  re_len;               	           //定义一个变量，存放接收的数据总长度
	int  pack_num;                         //定义一个变量，当多个推送一起过来时，保存推送的个数
    int  temp_len;                         //定义一个变量，暂存数据
    int  totle_len;                        //定义一个变量，存放已经统计的推送的总数据量
	int  topic_len;              	       //定义一个变量，存放推送中主题的长度
	int  cmd_len;                          //定义一个变量，存放推送中包含的命令数据的长度
	int  cmd_loca;                         //定义一个变量，存放推送中包含的命令的起始位置
	int  i;                                //定义一个变量，用于for循环
	int  local;
	unsigned char tempbuff[RBUFF_UNIT];	   //临时缓冲区
	unsigned char *data;                   //redata过来的时候，第一个字节是数据总量，data用于指向redata的第2个字节，真正的数据开始的地方
		
	re_len = redata[0] * 256 + redata[1];                 //获取接收的数据总长度		
	data = &redata[2];                                    //data指向redata的第2个字节，真正的数据开始的 
	pack_num = temp_len = totle_len = 0;                  //各个变量清零
	do{
		pack_num++;                                       //开始循环统计推送的个数，每次循环推送的个数+1	
		temp_len = mqtt_decode_remaining_length(&data[totle_len + 1], &local);
		totle_len += (temp_len + local + 1);              //累计统计的总的推送的数据长度
		re_len -= (temp_len + local + 1);                 //接收的数据总长度 减去 本次统计的推送的总长度
	}while(re_len!=0);                                    //如果接收的数据总长度等于0了，说明统计完毕了
	printf("RX publish packets: %d\r\n", pack_num);
	temp_len = totle_len = 0;                		      //各个变量清零
	for(i = 0; i < pack_num; i++)						  //已经统计到了接收的推送个数，开始for循环，取出每个推送的数据 
	{                                		
		temp_len = mqtt_decode_remaining_length(&data[totle_len + 1], &local);
		topic_len = data[totle_len + local + 1]*256 + data[totle_len + local + 2] + 2; //计算本次推送数据中主题占用的数据量
		cmd_len = temp_len - topic_len;                              			   //计算本次推送数据中命令数据占用的数据量
		cmd_loca = totle_len + local + topic_len + 1;               			   //计算本次推送数据中命令数据开始的位置
		if(cmd_len > RBUFF_UNIT)
			cmd_len = RBUFF_UNIT;
		memcpy(tempbuff, &data[cmd_loca], cmd_len);                   			   //命令数据拷贝出来		                 
		CMDBuf_Deal(tempbuff, cmd_len);                             			   //加入命令到缓冲区
		totle_len += (temp_len + local + 1);                         			   //累计已经统计的推送的数据长度
	}	
}

/*----------------------------------------------------------*/
/*函数名：处理发送缓冲区                                    */
/*参  数：data：数据                                        */
/*参  数：size：数据长度								    */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void TxDataBuf_Deal(unsigned char *data, int size)
{
	memcpy(&MQTT_TxDataInPtr[2], data, size);     //拷贝数据到发送缓冲区	
	MQTT_TxDataInPtr[0] = size/256;               //记录数据长度
	MQTT_TxDataInPtr[1] = size%256;               //记录数据长度
	MQTT_TxDataInPtr += TBUFF_UNIT;               //指针下移
	if(MQTT_TxDataInPtr == MQTT_TxDataEndPtr)     //如果指针到缓冲区尾部了
		MQTT_TxDataInPtr = MQTT_TxDataBuf[0];     //指针归位到缓冲区开头
}

/*----------------------------------------------------------*/
/*函数名：处理命令缓冲区									*/
/*参  数：data：数据                                        */
/*参  数：size：数据长度                                    */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void CMDBuf_Deal(unsigned char *data, int size)
{
	if(size > (CBUFF_UNIT - 3))
		size = CBUFF_UNIT - 3;

	memcpy(&MQTT_CMDInPtr[2], data,size);         //拷贝数据到命令缓冲区
	MQTT_CMDInPtr[0] = size/256;              	  //记录数据长度
	MQTT_CMDInPtr[1] = size%256;                  //记录数据长度
	MQTT_CMDInPtr[size+2] = '\0';                 //加入字符串结束符
	MQTT_CMDInPtr += CBUFF_UNIT;               	  //指针下移
	if(MQTT_CMDInPtr == MQTT_CMDEndPtr)           //如果指针到缓冲区尾部了
		MQTT_CMDInPtr = MQTT_CMDBuf[0];        	  //指针归位到缓冲区开头
}

/*----------------------------------------------------------*/
/*函数名：MQTT_ThresholdInit                                */
/*功  能：初始化阈值配置                                     */
/*说  明：优先从AT24C02读取阈值；若读取失败，写入默认值       */
/*参  数：无                                                */
/*返回值：无                                                */
/*----------------------------------------------------------*/
void MQTT_ThresholdInit(void)
{
	if(MQTT_LoadThresholdFromAT24C02() != 0)
	{
		g_mqtt_threshold.temperature_threshold = 30;
		g_mqtt_threshold.humidity_threshold = 80;
		g_mqtt_threshold.light_threshold = 300;
		MQTT_SaveThresholdToAT24C02();
	}
}

/*----------------------------------------------------------*/
/*函数名：MQTT_LoadThresholdFromAT24C02                     */
/*功  能：从AT24C02读取温湿度/光照阈值                       */
/*说  明：会检查magic/version/CRC和数值范围                 */
/*参  数：无                                                */
/*返回值：0成功，非0失败                                    */
/*----------------------------------------------------------*/
uint8_t MQTT_LoadThresholdFromAT24C02(void)
{
	uint8_t magic;
	uint8_t ver;
	uint8_t crc_s;
	uint8_t crc_c;
	int16_t t;
	int16_t h;
	uint16_t l;

	magic = AT24C_ReadByte(THRESHOLD_EE_MAGIC_ADDR);
	ver = AT24C_ReadByte(THRESHOLD_EE_VER_ADDR);
	if((magic != THRESHOLD_EE_MAGIC) || (ver != THRESHOLD_EE_VER))
		return 1;

	crc_s = AT24C_ReadByte(THRESHOLD_EE_CRC_ADDR);
	crc_c = threshold_crc_calc();
	if(crc_s != crc_c)
		return 2;

	t = (int16_t)((AT24C_ReadByte(THRESHOLD_EE_TEMP_H_ADDR) << 8) | AT24C_ReadByte(THRESHOLD_EE_TEMP_L_ADDR));
	h = (int16_t)((AT24C_ReadByte(THRESHOLD_EE_HUMI_H_ADDR) << 8) | AT24C_ReadByte(THRESHOLD_EE_HUMI_L_ADDR));
	l = (uint16_t)((AT24C_ReadByte(THRESHOLD_EE_LIGHT_H_ADDR) << 8) | AT24C_ReadByte(THRESHOLD_EE_LIGHT_L_ADDR));

	if((t < -40) || (t > 125) || (h < 0) || (h > 100) || (l > 20000))
		return 3;

	g_mqtt_threshold.temperature_threshold = t;
	g_mqtt_threshold.humidity_threshold = h;
	g_mqtt_threshold.light_threshold = l;

	return 0;
}

/*----------------------------------------------------------*/
/*函数名：MQTT_SaveThresholdToAT24C02                       */
/*功  能：把当前阈值写入AT24C02                             */
/*说  明：写入完成后同步更新CRC字节                          */
/*参  数：无                                                */
/*返回值：0成功，非0失败                                    */
/*----------------------------------------------------------*/
uint8_t MQTT_SaveThresholdToAT24C02(void)
{
	uint8_t ret;
	uint8_t crc;
	uint16_t t;
	uint16_t h;

	t = (uint16_t)g_mqtt_threshold.temperature_threshold;
	h = (uint16_t)g_mqtt_threshold.humidity_threshold;

	ret = 0;
	ret |= AT24C_WriteByte(THRESHOLD_EE_MAGIC_ADDR, THRESHOLD_EE_MAGIC);
	ret |= AT24C_WriteByte(THRESHOLD_EE_VER_ADDR, THRESHOLD_EE_VER);
	ret |= AT24C_WriteByte(THRESHOLD_EE_TEMP_L_ADDR, (uint8_t)(t & 0xFF));
	ret |= AT24C_WriteByte(THRESHOLD_EE_TEMP_H_ADDR, (uint8_t)((t >> 8) & 0xFF));
	ret |= AT24C_WriteByte(THRESHOLD_EE_HUMI_L_ADDR, (uint8_t)(h & 0xFF));
	ret |= AT24C_WriteByte(THRESHOLD_EE_HUMI_H_ADDR, (uint8_t)((h >> 8) & 0xFF));
	ret |= AT24C_WriteByte(THRESHOLD_EE_LIGHT_L_ADDR, (uint8_t)(g_mqtt_threshold.light_threshold & 0xFF));
	ret |= AT24C_WriteByte(THRESHOLD_EE_LIGHT_H_ADDR, (uint8_t)((g_mqtt_threshold.light_threshold >> 8) & 0xFF));

	crc = threshold_crc_calc();
	ret |= AT24C_WriteByte(THRESHOLD_EE_CRC_ADDR, crc);

	return ret;
}

/*----------------------------------------------------------*/
/*函数名：MQTT_ParseOneNetThresholdJson                     */
/*功  能：解析OneNet下发JSON并更新本地阈值                   */
/*说  明：支持多个字段名别名，更新成功后自动写入AT24C02       */
/*参  数：json_payload：云端下发的JSON字符串                 */
/*返回值：1有更新，0无更新                                   */
/*----------------------------------------------------------*/
uint8_t MQTT_ParseOneNetThresholdJson(const char *json_payload)
{
	int v;
	uint8_t updated;

	updated = 0;
	if(json_payload == 0)
		return 0;

	if(parse_json_int_by_key(json_payload, "\"temperature_threshold\"", &v) ||
	   parse_json_int_by_key(json_payload, "\"TemperatureThreshold\"", &v) ||
	   parse_json_int_by_key(json_payload, "\"TempThreshold\"", &v))
	{
		g_mqtt_threshold.temperature_threshold = (int16_t)clamp_int(v, -40, 125);
		updated = 1;
	}

	if(parse_json_int_by_key(json_payload, "\"humidity_threshold\"", &v) ||
	   parse_json_int_by_key(json_payload, "\"HumidityThreshold\"", &v) ||
	   parse_json_int_by_key(json_payload, "\"HumiThreshold\"", &v))
	{
		g_mqtt_threshold.humidity_threshold = (int16_t)clamp_int(v, 0, 100);
		updated = 1;
	}

	if(parse_json_int_by_key(json_payload, "\"light_threshold\"", &v) ||
	   parse_json_int_by_key(json_payload, "\"LightThreshold\"", &v) ||
	   parse_json_int_by_key(json_payload, "\"IlluminanceThreshold\"", &v))
	{
		g_mqtt_threshold.light_threshold = (uint16_t)clamp_int(v, 0, 20000);
		updated = 1;
	}

	if(updated)
		MQTT_SaveThresholdToAT24C02();

	return updated;
}

/*----------------------------------------------------------*/
/*函数名：MQTT_ProcessOneNetThresholdCommand                */
/*功  能：处理命令缓冲区头部的一条阈值命令                   */
/*说  明：处理后无论成功与否都会将命令出队，避免阻塞后续命令   */
/*参  数：无                                                */
/*返回值：1有更新，0无更新                                   */
/*----------------------------------------------------------*/
uint8_t MQTT_ProcessOneNetThresholdCommand(void)
{
	uint8_t updated;

	if(MQTT_CMDOutPtr == MQTT_CMDInPtr)
		return 0;

	updated = MQTT_ParseOneNetThresholdJson((const char *)&MQTT_CMDOutPtr[2]);

	MQTT_CMDOutPtr += CBUFF_UNIT;
	if(MQTT_CMDOutPtr == MQTT_CMDEndPtr)
		MQTT_CMDOutPtr = MQTT_CMDBuf[0];

	return updated;
}
