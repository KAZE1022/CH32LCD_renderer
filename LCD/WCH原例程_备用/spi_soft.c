/*********************************************************************
 * File Name          : spi_soft.c
 * Description        : ST7735S 软件 SPI 驱动 (PC4 = DIN, PC5 = CLK)
 *
 *   时序: 空闲低电平, 数据在上升沿被 ST7735S 采样, MSB first。
 *
 *   为了速度, 这里把"置 CLK 高 + 置 CLK 低"合成一次 32 位寄存器写:
 *       BSHR 写 1  -> 置高
 *       BCR  写 1  -> 置低
 *   所以每一位只需要 2 次 IO 写, 144MHz 下约 12MHz 时钟。
 *******************************************************************************/

#include "spi_soft.h"

/* 位延时: 0 个空指令也不影响功能, 只是让时钟更快; 见 config.h 说明 */
#if (LCD_SPI_DELAY > 0)
#define SW_SPI_DELAY()                                     \
    do {                                                   \
        volatile uint32_t _i = (LCD_SPI_DELAY);            \
        while (_i--) { __asm volatile("nop"); }            \
    } while (0)
#else
#define SW_SPI_DELAY()  do { } while (0)
#endif

#define SW_SPI_SCK_HIGH()   (LCD_PORT->BSHR = LCD_SCK_PIN)
#define SW_SPI_SCK_LOW()    (LCD_PORT->BCR  = LCD_SCK_PIN)
#define SW_SPI_MOSI_HIGH()  (LCD_PORT->BSHR = LCD_MOSI_PIN)
#define SW_SPI_MOSI_LOW()   (LCD_PORT->BCR  = LCD_MOSI_PIN)

/* CH32V30x GPIO: BSHR 偏移 0x10(置位) / BCR 偏移 0x14(清零) */
#define SW_SPI_PORT_SET(mask)   (*(volatile uint32_t *)((uint32_t)LCD_PORT + 0x10) = (mask))
#define SW_SPI_PORT_CLR(mask)   (*(volatile uint32_t *)((uint32_t)LCD_PORT + 0x14) = (mask))
/* 一次写同时决定 CLK 和数据位: clk=1 且 data=1 -> 两位置位; 否则只置 CLK */
#define SW_SPI_BIT(val)                                                        \
    do {                                                                       \
        SW_SPI_PORT_SET(((val) & 0x80) ? (LCD_SCK_PIN | LCD_MOSI_PIN) : LCD_SCK_PIN); \
        SW_SPI_DELAY();                                                        \
        SW_SPI_PORT_CLR(LCD_SCK_PIN | LCD_MOSI_PIN);                           \
        SW_SPI_DELAY();                                                        \
        (val) <<= 1;                                                           \
    } while (0)

/*********************************************************************
 * @fn      SPI_Soft_Init
 * @brief   配置 DIN/CLK 为推挽输出, 并把 CLK 拉低 (SPI Mode 0 空闲态)
 *********************************************************************/
void SPI_Soft_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(LCD_PORT_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = LCD_MOSI_PIN | LCD_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_PORT, &GPIO_InitStructure);

    SW_SPI_SCK_LOW();
    SW_SPI_MOSI_LOW();
}

/*********************************************************************
 * @fn      SPI_Soft_WriteByte
 * @brief   软件 SPI 发送 1 字节, MSB first
 *
 *   循环展开 8 位, 每位: 先按数据位设置 MOSI, 再拉高 CLK 采样, 再拉低 CLK
 *********************************************************************/
void SPI_Soft_WriteByte(uint8_t data)
{
    uint32_t val = data;

    /* 8 位循环展开: 每位先定好 MOSI, 再拉高 CLK 让 ST7735S 采样, 然后拉低 */
    SW_SPI_BIT(val);
    SW_SPI_BIT(val);
    SW_SPI_BIT(val);
    SW_SPI_BIT(val);
    SW_SPI_BIT(val);
    SW_SPI_BIT(val);
    SW_SPI_BIT(val);
    SW_SPI_BIT(val);

    SW_SPI_SCK_LOW();
}

/*********************************************************************
 * @fn      SPI_Soft_WriteBytes
 * @brief   连续发送 len 字节
 *********************************************************************/
void SPI_Soft_WriteBytes(const uint8_t *data, uint32_t len)
{
    while (len--)
    {
        SPI_Soft_WriteByte(*data++);
    }
}

/*********************************************************************
 * @fn      SPI_Soft_WriteRepeat
 * @brief   同一个字节重复发送 len 次
 *********************************************************************/
void SPI_Soft_WriteRepeat(uint8_t data, uint32_t len)
{
    while (len--)
    {
        SPI_Soft_WriteByte(data);
    }
}
