/*********************************************************************
 * File Name          : lcd_port.h
 * Description        : LCD 的底层输出层 —— 屏蔽"硬件 SPI2"和"软件模拟 SPI"的区别
 *
 *  上层(lcd.c / fps.c)只调用这里的接口, 不关心底层是怎么发字节的。
 *  切换方式: config.h 里的 LCD_USE_HARD_SPI (1=硬件, 0=软件)
 *
 *  一次传输的流程(CS 只在整段的首尾动一次)：
 *      LCD_PORT_BeginCmd();
 *      LCD_PORT_WriteByte(cmd);
 *      LCD_PORT_BeginData();
 *      LCD_PORT_WriteBlock(data, len);
 *      LCD_PORT_End();
 *******************************************************************************/
#ifndef __LCD_PORT_H__
#define __LCD_PORT_H__

#include "debug.h"
#include "config.h"

#if (LCD_USE_HARD_SPI)
#include "ch32v30x_spi.h"
#endif

/*---------------------------------------------------------------------------
 * 初始化 / 复位 / 背光
 *-------------------------------------------------------------------------*/
void LCD_PORT_Init(void);            /* 配置引脚 + SPI(或软件 SPI) */
void LCD_PORT_ResetPin(void);        /* 硬复位时序 */
void LCD_PORT_BackLight(uint8_t on); /* 背光 */

/*---------------------------------------------------------------------------
 * 片选 / 命令数据切换
 *-------------------------------------------------------------------------*/
void LCD_PORT_BeginCmd(void);        /* CS=0, DC=0 */
void LCD_PORT_BeginData(void);       /* CS=0, DC=1 */
void LCD_PORT_End(void);             /* CS=1 */

/*---------------------------------------------------------------------------
 * 写数据
 *-------------------------------------------------------------------------*/
void LCD_PORT_WriteByte(uint8_t data);
void LCD_PORT_WriteBlock(const uint8_t *data, uint32_t len);   /* 大块数据 */
void LCD_PORT_WriteRepeat(uint16_t color, uint32_t count);     /* 重复像素 */

/*---------------------------------------------------------------------------
 * 内部用的大块搬运缓冲(拼好一块再整块发)
 *-------------------------------------------------------------------------*/
extern uint16_t LCD_BulkBuf[LCD_BULK_PIXELS];

#endif /* __LCD_PORT_H__ */
