#ifndef __BH1750_H
#define __BH1750_H

/**
 * BH1750 从机地址定义
 * ADDR引脚接地时为 0x23，左移一位得到写地址 0x46，读地址 0x47
 * 这里统一使用 0x46，库函数内部会根据 Direction 自动处理最低位
 */
#define BH1750_ADDR    0x46 

/* 常用命令定义 */
#define BH1750_CMD_POWER_ON   0x01
#define BH1750_CMD_RESET      0x07
#define BH1750_CMD_H_RES_MODE 0x10  // 1lx分辨率，连续测量模式

// 硬件I2C2: 光照传感器BH1750
#define BH1750_GPIO_CLK       RCC_APB2Periph_GPIOB
#define BH1750_I2C_CLK        RCC_APB1Periph_I2C2
#define BH1750_I2C            I2C2
#define BH1750_GPIO           GPIOB
#define BH1750_SCL_PIN        GPIO_Pin_10
#define BH1750_SDA_PIN        GPIO_Pin_11

void BH1750_Init(void);
/* 调用顺序建议：BH1750_Init() -> BH1750_Init_Config() -> 周期调用 BH1750_Read_Lux() */
void BH1750_Init_Config(void);
/* 返回滤波后的光照值（单位：lux） */
float BH1750_Read_Lux(void);

#endif
