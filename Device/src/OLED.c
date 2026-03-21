#include "stm32f10x.h"                  // Device header
#include "OLED_Font.h"
#include "OLED.h"
#include "Delay.h"

/* 底层位控制宏 */
#define OLED_W_SCL(x)    GPIO_WriteBit(OLED_GPIO, OLED_SCL_PIN, (BitAction)(x))
#define OLED_W_SDA(x)    GPIO_WriteBit(OLED_GPIO, OLED_SDA_PIN, (BitAction)(x))

/*  模拟 I2C 起始信号 */
// SDA 先置1→ SCL 置1 → SDA 拉低 → SCL 拉低
static void OLED_I2C_Start(void) 
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_W_SDA(0);
    OLED_W_SCL(0);
}

/*  模拟 I2C 停止信号 */
// SDA 先置0 → SCL 置1 → SDA 拉高
static void OLED_I2C_Stop(void) 
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

/*  发送字节（带临界区保护，确保脉冲宽度不被任务切换打断） */
static void OLED_I2C_SendByte(uint8_t Byte) 
{
    uint8_t i;
	
    for (i = 0; i < 8; i++) {
        OLED_W_SDA(!!(Byte & (0x80 >> i)));
        OLED_W_SCL(1);
        OLED_W_SCL(0);
    }
    OLED_W_SCL(1); // 接收应答的时钟脉冲（模拟，不读取实际应答）
    OLED_W_SCL(0);
}
//OLED写命令, Command 是要写入的命令
void OLED_WriteCommand(uint8_t Command) 
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78); // 从机地址
    OLED_I2C_SendByte(0x00); // 写命令
    OLED_I2C_SendByte(Command);
    OLED_I2C_Stop();
}

//OLED写数据, Data 是要写入的数据
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
  * @brief  设置光标位置（显存坐标）
  * @param  Y: 页地址，以左上角为原点，向下方向的坐标 (0~7)
  * @param  X: 列地址，以左上角为原点，向右方向的坐标 (0~127)
  */
static void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					// 设置页地址
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	// 设置列地址高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			// 设置列地址低4位
}

/**
  * @brief  全屏清屏
  */
void OLED_Clear(void)
{
	uint8_t i, j;

    for (j = 0; j < 8; j++) // 8个页
	{
        OLED_SetCursor(j, 0);
        for (i = 0; i < 128; i++) // 128列
		{
            OLED_WriteData(0x00); // 写入空数据
		}
	}
}

// 指定行、列显示单个 ASCII 字符(不带锁)
static void OLED_ShowChar(uint8_t Line, uint8_t Column, uint8_t Char)
{
    uint8_t i;
	
	  OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	  for (i = 0; i < 8; i++)
	  {
		  OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//显示上半部分内容
			
	  }
	  OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	  for (i = 0; i < 8; i++)
	  {
		  OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//显示下半部分内容
	  }
 
}

// OLED显示字符串
void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String)
{
    uint8_t i;

    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

// 辅助函数：计算次方，返回值等于 X 的 Y 次方
static uint32_t OLED_Pow(uint32_t m, uint32_t n)
{
	uint32_t result = 1;
	while (n--) result *= m;
	return result;
}

// 显示十进制正整数
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;

    for (i = 0; i < Length; i++)							
    {
        OLED_ShowChar(Line, Column + i , Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}


void OLED_Init(void) 
{
    /* 初始化 GPIO */
    RCC_APB2PeriphClockCmd(OLED_GPIO_CLK, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_Init(OLED_GPIO, &GPIO_InitStructure);
    OLED_W_SCL(1); OLED_W_SDA(1);

    Delay_ms(100); // 屏幕上电预留稳定时间

    /* SSD1306 标准初始化序列 */
    OLED_WriteCommand(0xAE); // 关闭显示
    OLED_WriteCommand(0xD5); // 设置显示时钟分频比/振荡器频率
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8); // 设置多路复用率
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3); // 设置显示偏移
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40); // 设置显示开始行
    OLED_WriteCommand(0x8D); // 电荷泵设置
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0x20); // 设置内存地址模式
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0xA1); // 设置段重映射
    OLED_WriteCommand(0xC8); // 设置COM扫描方向
    OLED_WriteCommand(0xDA); // 设置COM硬件引脚配置
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81); // 设置对比度
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9); // 设置预充电周期
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB); // 设置VCOMH取消选择级别
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0xA4); // 禁用整个显示开启
    OLED_WriteCommand(0xA6); // 设置正常显示
    OLED_WriteCommand(0xAF); // 开启显示
		
    OLED_Clear();
}
