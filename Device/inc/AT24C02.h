#ifndef __AT24C_H
#define __AT24C_H

#include "stdint.h"


// AT24C02 从机地址: A0,A1,A2接地为0xA0
#define AT24C_ADDR           0xA0 
#define AT24C_PAGE_SIZE      8      // AT24C02 每页8字节
#define AT24C_TOTAL_SIZE     256    // AT24C02 总容量256字节

// 硬件I2C1: EEPROM存储器AT24C02
#define AT24C_GPIO_CLK        RCC_APB2Periph_GPIOB
#define AT24C_I2C_CLK         RCC_APB1Periph_I2C1
#define AT24C_I2C             I2C1
#define AT24C_GPIO            GPIOB
#define AT24C_SCL_PIN         GPIO_Pin_6
#define AT24C_SDA_PIN         GPIO_Pin_7

void AT24C_Init(void);
/* 说明：本驱动为“强制初始化模式”，调用任意读写接口前必须先调用 AT24C_Init() 一次 */
uint8_t AT24C_WriteByte(uint8_t WordAddress, uint8_t Data);
uint8_t AT24C_ReadByte(uint8_t WordAddress);
uint8_t AT24C_WriteBuffer(uint8_t WordAddress, const uint8_t *pData, uint16_t length);
uint8_t AT24C_ReadBuffer(uint8_t WordAddress, uint8_t *pData, uint16_t length);
uint8_t AT24C_CheckDevice(void);

#endif
