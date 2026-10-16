/*********************************************************************
 * File Name          : spi_soft.h
 * Description        : ST7735S 用的软件(IO 模拟)SPI 驱动, 无需外设 SPI
 *
 *   引脚见 config.h:
 *       SCK  = PC5
 *       MOSI = PC4
 *   只实现"只写"方向 (LCD 单向), 速度约 10~12MHz @144MHz 主频。
 *******************************************************************************/
#ifndef __SPI_SOFT_H__
#define __SPI_SOFT_H__

#include "debug.h"
#include "config.h"

/* 初始化 SCK / MOSI 为推挽输出, 并把时钟拉低 */
void SPI_Soft_Init(void);

/* 发送 1 字节 (MSB first) */
void SPI_Soft_WriteByte(uint8_t data);

/* 连续发送 len 字节, 用于刷屏/写字等大块数据 */
void SPI_Soft_WriteBytes(const uint8_t *data, uint32_t len);

/* 重复发送同一个字节 len 次 (例如清屏) */
void SPI_Soft_WriteRepeat(uint8_t data, uint32_t len);

#endif /* __SPI_SOFT_H__ */
