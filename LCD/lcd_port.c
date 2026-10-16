/*********************************************************************
 * File Name          : lcd_port.c
 * Description        : LCD 底层输出层实现
 *
 *   两种模式二选一(config.h 的 LCD_USE_HARD_SPI):
 *
 *   硬件 SPI2 (默认, 快):
 *       SCL = PB13 (SPI2_SCK)
 *       SDA = PB15 (SPI2_MOSI)
 *       CS  = PB12 (普通 IO, 软件控制)
 *       DC  = PB14 (普通 IO)
 *       SPI2 挂 APB1 = 48MHz, 默认 2 分频 -> 24MHz
 *       数据可以走 DMA1_Channel5, 刷屏时 CPU 完全不用管
 *
 *   软件模拟 SPI (任何引脚都行, 慢):
 *       直接翻转 IO, 约 10~12MHz @144MHz
 *******************************************************************************/

#include "lcd_port.h"

/* 大块搬运缓冲 */
uint16_t LCD_BulkBuf[LCD_BULK_PIXELS];

/*===========================================================================
 *  引脚操作宏
 *=========================================================================*/
#define CS_SET()    (LCD_CS_PORT->BSHR = LCD_CS_PIN)
#define CS_CLR()    (LCD_CS_PORT->BCR  = LCD_CS_PIN)
#define DC_SET()    (LCD_DC_PORT->BSHR = LCD_DC_PIN)
#define DC_CLR()    (LCD_DC_PORT->BCR  = LCD_DC_PIN)
#define RST_SET()   (LCD_RST_PORT->BSHR = LCD_RST_PIN)
#define RST_CLR()   (LCD_RST_PORT->BCR  = LCD_RST_PIN)

#if (LCD_USE_HARD_SPI)
/*---------------------------------------------------------------------------
 *  硬件 SPI2 + DMA
 *-------------------------------------------------------------------------*/
#if (LCD_USE_DMA)
#define LCD_DMA_TX_CH     DMA1_Channel5
#define LCD_DMA_TC_FLAG   DMA1_FLAG_TC5
static volatile uint8_t LCD_DmaBusy = 0;

static void LCD_DMA_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_DeInit(LCD_DMA_TX_CH);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)LCD_BulkBuf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize         = 0;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(LCD_DMA_TX_CH, &DMA_InitStructure);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
}

/* 等 DMA 发完 */
static void LCD_DMA_Wait(void)
{
    if (!LCD_DmaBusy)
    {
        return;
    }
    while (DMA_GetFlagStatus(LCD_DMA_TC_FLAG) == RESET)
        ;
    DMA_ClearFlag(LCD_DMA_TC_FLAG);
    LCD_DmaBusy = 0;
}
#endif /* LCD_USE_DMA */

/* 硬件 SPI 阻塞式发一个字节 */
static void SPI_HW_WriteByte(uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
        ;
    SPI_I2S_SendData(SPI2, data);
}

/* 硬件 SPI 阻塞式发一块 */
static void SPI_HW_WriteBlock(const uint8_t *data, uint32_t len)
{
    while (len--)
    {
        SPI_HW_WriteByte(*data++);
    }
}

void LCD_PORT_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef  SPI_InitStructure  = {0};

    /* --- 时钟 --- */
    RCC_APB2PeriphClockCmd(LCD_CS_RCC | LCD_DC_RCC | LCD_RST_RCC |
                           LCD_BL_RCC | LCD_SPI_GPIO_RCC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* --- SCK(PB13) / MOSI(PB15) 复用推挽 --- */
    GPIO_InitStructure.GPIO_Pin   = LCD_SCK_PIN | LCD_MOSI_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* --- CS / DC / RST / BL 普通推挽输出 --- */
    GPIO_InitStructure.GPIO_Pin   = LCD_CS_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(LCD_CS_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = LCD_DC_PIN;
    GPIO_Init(LCD_DC_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = LCD_RST_PIN;
    GPIO_Init(LCD_RST_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = LCD_BL_PIN;
    GPIO_Init(LCD_BL_PORT, &GPIO_InitStructure);

    CS_SET();
    DC_SET();
    RST_SET();
#if (LCD_BL_ACTIVE_HIGH)
    LCD_BL_PORT->BCR = LCD_BL_PIN;      /* 先关背光 */
#else
    LCD_BL_PORT->BSHR = LCD_BL_PIN;
#endif

    /* --- SPI2: 主机, 只发, 模式 0 (CPOL=0 CPHA=1Edge), MSB first --- */
    SPI_InitStructure.SPI_Direction         = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = LCD_SPI_PRESCALER;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI2, &SPI_InitStructure);

    SPI_Cmd(SPI2, ENABLE);

#if (LCD_USE_DMA)
    LCD_DMA_Init();
#endif
}

void LCD_PORT_BeginCmd(void)
{
#if (LCD_USE_DMA)
    LCD_DMA_Wait();
#endif
    CS_CLR();
    DC_CLR();
}

void LCD_PORT_BeginData(void)
{
#if (LCD_USE_DMA)
    LCD_DMA_Wait();
#endif
    CS_CLR();
    DC_SET();
}

void LCD_PORT_End(void)
{
#if (LCD_USE_DMA)
    /* 等 DMA 真的发完再放 CS, 否则最后一个字节会被截掉 */
    LCD_DMA_Wait();
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET)
        ;
#endif
    CS_SET();
}

void LCD_PORT_WriteByte(uint8_t data)
{
    SPI_HW_WriteByte(data);
}

void LCD_PORT_WriteBlock(const uint8_t *data, uint32_t len)
{
#if (LCD_USE_DMA)
    if (len >= 16u)
    {
        LCD_DMA_Wait();
        DMA_Cmd(LCD_DMA_TX_CH, DISABLE);
        DMA_ClearFlag(LCD_DMA_TC_FLAG);
        LCD_DMA_TX_CH->MADDR = (uint32_t)data;
        LCD_DMA_TX_CH->CNTR  = (uint16_t)len;
        LCD_DmaBusy = 1;
        DMA_Cmd(LCD_DMA_TX_CH, ENABLE);
        return;
    }
    /* 太短就没必要动用 DMA, 阻塞发更快 */
#endif
    SPI_HW_WriteBlock(data, len);
}

void LCD_PORT_WriteRepeat(uint16_t color, uint32_t count)
{
    uint32_t i;
    uint8_t  hi = (uint8_t)(color >> 8);
    uint8_t  lo = (uint8_t)color;

    /* 先在内存里拼满一块, 再整块发, 比一个字节一个字节发快 */
    while (count > 0)
    {
        uint32_t n = (count > LCD_BULK_PIXELS) ? LCD_BULK_PIXELS : count;

        for (i = 0; i < n; i++)
        {
            LCD_BulkBuf[i] = color;
        }
        (void)hi;
        (void)lo;
        LCD_PORT_WriteBlock((const uint8_t *)LCD_BulkBuf, n * 2u);
        count -= n;
    }
}

#else  /* =============== 软件模拟 SPI =============== */
/*---------------------------------------------------------------------------
 *  软件 SPI: 需要额外的 SCK / MOSI 引脚配置
 *  硬件 SPI 模式下 SCK/MOSI 是 SPI2 的固定复用脚(PB13/PB15),
 *  软件模式下改成 config.h 里可以自由指定的两个 IO。
 *-------------------------------------------------------------------------*/
#ifndef LCD_SW_SCK_PORT
#define LCD_SW_SCK_PORT    GPIOB
#define LCD_SW_SCK_PIN     GPIO_Pin_13
#define LCD_SW_MOSI_PORT   GPIOB
#define LCD_SW_MOSI_PIN    GPIO_Pin_15
#define LCD_SW_GPIO_RCC    RCC_APB2Periph_GPIOB
#endif

#if (LCD_SPI_DELAY > 0)
#define SW_DELAY()                                              \
    do {                                                        \
        volatile uint32_t _i = (LCD_SPI_DELAY);                 \
        while (_i--) { __asm volatile("nop"); }                 \
    } while (0)
#else
#define SW_DELAY()  do { } while (0)
#endif

#define SW_SCK_HIGH()    (LCD_SW_SCK_PORT->BSHR = LCD_SW_SCK_PIN)
#define SW_SCK_LOW()     (LCD_SW_SCK_PORT->BCR  = LCD_SW_SCK_PIN)
#define SW_MOSI_HIGH()   (LCD_SW_MOSI_PORT->BSHR = LCD_SW_MOSI_PIN)
#define SW_MOSI_LOW()    (LCD_SW_MOSI_PORT->BCR  = LCD_SW_MOSI_PIN)

static void SW_WriteByte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8u; i++)
    {
        if (data & 0x80u)
        {
            SW_MOSI_HIGH();
        }
        else
        {
            SW_MOSI_LOW();
        }
        SW_DELAY();
        SW_SCK_HIGH();
        SW_DELAY();
        SW_SCK_LOW();
        data = (uint8_t)(data << 1);
    }
}

void LCD_PORT_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(LCD_CS_RCC | LCD_DC_RCC | LCD_RST_RCC |
                           LCD_BL_RCC | LCD_SW_GPIO_RCC, ENABLE);

    /* SCK / MOSI */
    GPIO_InitStructure.GPIO_Pin   = LCD_SW_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_SW_SCK_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = LCD_SW_MOSI_PIN;
    GPIO_Init(LCD_SW_MOSI_PORT, &GPIO_InitStructure);

    /* CS / DC / RST / BL */
    GPIO_InitStructure.GPIO_Pin   = LCD_CS_PIN;
    GPIO_Init(LCD_CS_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin   = LCD_DC_PIN;
    GPIO_Init(LCD_DC_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin   = LCD_RST_PIN;
    GPIO_Init(LCD_RST_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin   = LCD_BL_PIN;
    GPIO_Init(LCD_BL_PORT, &GPIO_InitStructure);

    SW_SCK_LOW();
    SW_MOSI_LOW();

    CS_SET();
    DC_SET();
    RST_SET();
#if (LCD_BL_ACTIVE_HIGH)
    LCD_BL_PORT->BCR = LCD_BL_PIN;
#else
    LCD_BL_PORT->BSHR = LCD_BL_PIN;
#endif
}

void LCD_PORT_BeginCmd(void)
{
    CS_CLR();
    DC_CLR();
}

void LCD_PORT_BeginData(void)
{
    CS_CLR();
    DC_SET();
}

void LCD_PORT_End(void)
{
    CS_SET();
}

void LCD_PORT_WriteByte(uint8_t data)
{
    SW_WriteByte(data);
}

void LCD_PORT_WriteBlock(const uint8_t *data, uint32_t len)
{
    while (len--)
    {
        SW_WriteByte(*data++);
    }
}

void LCD_PORT_WriteRepeat(uint16_t color, uint32_t count)
{
    uint32_t i;

    while (count > 0)
    {
        uint32_t n = (count > LCD_BULK_PIXELS) ? LCD_BULK_PIXELS : count;

        for (i = 0; i < n; i++)
        {
            LCD_BulkBuf[i] = color;
        }
        LCD_PORT_WriteBlock((const uint8_t *)LCD_BulkBuf, n * 2u);
        count -= n;
    }
}
#endif /* LCD_USE_HARD_SPI */

/*===========================================================================
 *  复位 / 背光(两种模式共用)
 *=========================================================================*/
void LCD_PORT_ResetPin(void)
{
    RST_SET();
    Delay_Ms(10);
    RST_CLR();
    Delay_Ms(20);
    RST_SET();
    Delay_Ms(120);
}

void LCD_PORT_BackLight(uint8_t on)
{
#if (LCD_BL_ACTIVE_HIGH)
    if (on) { LCD_BL_PORT->BSHR = LCD_BL_PIN; } else { LCD_BL_PORT->BCR = LCD_BL_PIN; }
#else
    if (on) { LCD_BL_PORT->BCR  = LCD_BL_PIN; } else { LCD_BL_PORT->BSHR = LCD_BL_PIN; }
#endif
}
