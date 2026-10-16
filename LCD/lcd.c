/*********************************************************************
 * File Name          : lcd.c
 * Description        : ST7735S (1.8寸 128x160) 驱动
 *
 *   接线(见 config.h):
 *      SCL = PB13(SPI2_SCK)   SDA = PB15(SPI2_MOSI)
 *      CS  = PB12             DC  = PB14
 *      RST = PC4              BLK = PC5
 *
 *   底层字节收发由 lcd_port.c 负责(硬件 SPI2 + DMA, 或软件模拟 SPI)。
 *   本文件只管 ST7735S 的寄存器、窗口、画点/填充/图形/位图。
 *
 *   显示有偏移时改 config.h 的 LCD_X_OFFSET / LCD_Y_OFFSET,
 *   或者把 main.c 的 LCD_OFFSET_TEST_MODE 设成 1 让屏幕自己试 4 组。
 *******************************************************************************/

#include "lcd.h"

/*---------------------------------------------------------------------------
 * ST7735S 命令
 *-------------------------------------------------------------------------*/
#define ST7735_SWRESET   0x01
#define ST7735_SLPOUT    0x11
#define ST7735_NORON     0x13
#define ST7735_DISPOFF   0x28
#define ST7735_DISPON    0x29
#define ST7735_CASET     0x2A
#define ST7735_RASET     0x2B
#define ST7735_RAMWR     0x2C
#define ST7735_MADCTL    0x36
#define ST7735_COLMOD    0x3A
#define ST7735_FRMCTR1   0xB1
#define ST7735_FRMCTR2   0xB2
#define ST7735_FRMCTR3   0xB3
#define ST7735_INVCTR    0xB4
#define ST7735_PWCTR1    0xC0
#define ST7735_PWCTR2    0xC1
#define ST7735_PWCTR3    0xC2
#define ST7735_PWCTR4    0xC3
#define ST7735_PWCTR5    0xC4
#define ST7735_VMCTR1    0xC5
#define ST7735_GMCTRP1   0xE0
#define ST7735_GMCTRN1   0xE1

/* MADCTL 位定义 */
#define MADCTL_MY   0x80
#define MADCTL_MX   0x40
#define MADCTL_MV   0x20
#define MADCTL_BGR  0x08

/*---------------------------------------------------------------------------
 * 内部状态
 *-------------------------------------------------------------------------*/
static uint16_t LCD_OffX = LCD_X_OFFSET;      /* 显示区 X 偏移 */
static uint16_t LCD_OffY = LCD_Y_OFFSET;      /* 显示区 Y 偏移 */
static uint16_t LCD_Width  = LCD_PHY_W;
static uint16_t LCD_Height = LCD_PHY_H;

/*===========================================================================
 *  低层: 写命令 / 写数据
 *=========================================================================*/
void LCD_WriteCommand(uint8_t cmd)
{
    LCD_PORT_BeginCmd();
    LCD_PORT_WriteByte(cmd);
    LCD_PORT_End();
}

void LCD_WriteData8(uint8_t data)
{
    LCD_PORT_BeginData();
    LCD_PORT_WriteByte(data);
    LCD_PORT_End();
}

void LCD_WriteCommandData(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    LCD_PORT_BeginCmd();
    LCD_PORT_WriteByte(cmd);
    if (len)
    {
        LCD_PORT_BeginData();
        for (i = 0; i < len; i++)
        {
            LCD_PORT_WriteByte(data[i]);
        }
    }
    LCD_PORT_End();
}

/*===========================================================================
 *  复位 / 背光
 *=========================================================================*/
void LCD_Reset(void)
{
    LCD_PORT_ResetPin();
}

void LCD_BackLight(uint8_t on)
{
    LCD_PORT_BackLight(on);
}

void LCD_DisplayOn(void)
{
    LCD_WriteCommand(ST7735_DISPON);
}

void LCD_DisplayOff(void)
{
    LCD_WriteCommand(ST7735_DISPOFF);
}

/*===========================================================================
 *  初始化
 *=========================================================================*/
void LCD_Init(void)
{
    static const uint8_t frmctr1[] = {0x05, 0x3C, 0x3C};
    static const uint8_t frmctr2[] = {0x05, 0x3C, 0x3C};
    static const uint8_t frmctr3[] = {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C};
    static const uint8_t invctr[]  = {0x03};
    static const uint8_t pwctr1[]  = {0x28, 0x08, 0x04};
    static const uint8_t pwctr2[]  = {0xC0};
    static const uint8_t pwctr3[]  = {0x0D, 0x00};
    static const uint8_t pwctr4[]  = {0x8D, 0x2A};
    static const uint8_t pwctr5[]  = {0x8D, 0xEE};
    static const uint8_t vmctr1[]  = {0x1A};
    static const uint8_t gmctrp1[] = {0x04, 0x22, 0x07, 0x0A, 0x2E, 0x30, 0x25, 0x2A,
                                      0x28, 0x26, 0x2E, 0x3A, 0x00, 0x01, 0x03, 0x13};
    static const uint8_t gmctrn1[] = {0x04, 0x16, 0x06, 0x0D, 0x2D, 0x26, 0x23, 0x27,
                                      0x27, 0x25, 0x2D, 0x3B, 0x00, 0x01, 0x04, 0x13};
    static const uint8_t colmod[]  = {0x05};      /* 16bit RGB565 */

    /* 1) 引脚 + SPI(硬件 SPI2 或软件模拟) */
    LCD_PORT_Init();

    /* 2) 硬件复位 */
    LCD_Reset();

    /* 3) 软复位 */
    LCD_WriteCommand(ST7735_SWRESET);
    Delay_Ms(120);

    /* 4) 退出睡眠 */
    LCD_WriteCommand(ST7735_SLPOUT);
    Delay_Ms(120);

    /* 5) 帧率 / 电源 / VCOM / Gamma */
    LCD_WriteCommandData(ST7735_FRMCTR1, frmctr1, sizeof(frmctr1));
    LCD_WriteCommandData(ST7735_FRMCTR2, frmctr2, sizeof(frmctr2));
    LCD_WriteCommandData(ST7735_FRMCTR3, frmctr3, sizeof(frmctr3));
    LCD_WriteCommandData(ST7735_INVCTR,  invctr,  sizeof(invctr));
    LCD_WriteCommandData(ST7735_PWCTR1,  pwctr1,  sizeof(pwctr1));
    LCD_WriteCommandData(ST7735_PWCTR2,  pwctr2,  sizeof(pwctr2));
    LCD_WriteCommandData(ST7735_PWCTR3,  pwctr3,  sizeof(pwctr3));
    LCD_WriteCommandData(ST7735_PWCTR4,  pwctr4,  sizeof(pwctr4));
    LCD_WriteCommandData(ST7735_PWCTR5,  pwctr5,  sizeof(pwctr5));
    LCD_WriteCommandData(ST7735_VMCTR1,  vmctr1,  sizeof(vmctr1));
    LCD_WriteCommandData(ST7735_GMCTRP1, gmctrp1, sizeof(gmctrp1));
    LCD_WriteCommandData(ST7735_GMCTRN1, gmctrn1, sizeof(gmctrn1));

    /* 6) 16 位色 */
    LCD_WriteCommandData(ST7735_COLMOD, colmod, sizeof(colmod));

    /* 7) 正常显示模式 + 开显示 */
    LCD_WriteCommand(ST7735_NORON);
    Delay_Ms(10);
    LCD_WriteCommand(ST7735_DISPON);
    Delay_Ms(100);

    /* 8) 显示方向 */
    LCD_Direction(2);

    /* 9) 清屏 + 点亮背光 */
    LCD_Clear(LCD_BLACK);
    LCD_BackLight(1);
}

/*===========================================================================
 *  显示方向
 *=========================================================================*/
void LCD_Direction(uint8_t direction)
{
    uint8_t madctl;

    switch (direction)
    {
        case 1:                                   /* 横屏 160x128 */
            LCD_Width  = LCD_PHY_H;
            LCD_Height = LCD_PHY_W;
            madctl     = MADCTL_MX | MADCTL_MV | MADCTL_BGR;
            break;

        case 2:                                   /* 竖屏翻转 */
            LCD_Width  = LCD_PHY_W;
            LCD_Height = LCD_PHY_H;
            madctl     = MADCTL_MX | MADCTL_MY | MADCTL_BGR;
            break;

        case 3:                                   /* 横屏翻转 */
            LCD_Width  = LCD_PHY_H;
            LCD_Height = LCD_PHY_W;
            madctl     = MADCTL_MY | MADCTL_MV | MADCTL_BGR;
            break;

        case 0:
        default:                                  /* 竖屏 128x160 */
            LCD_Width  = LCD_PHY_W;
            LCD_Height = LCD_PHY_H;
            madctl     = MADCTL_BGR;
            break;
    }

    LCD_WriteCommandData(ST7735_MADCTL, &madctl, 1);

    /* 横屏时 GRAM 的 X/Y 对调, 偏移也要跟着对调 */
    if (direction == 1 || direction == 3)
    {
        LCD_OffX = LCD_Y_OFFSET;
        LCD_OffY = LCD_X_OFFSET;
    }
    else
    {
        LCD_OffX = LCD_X_OFFSET;
        LCD_OffY = LCD_Y_OFFSET;
    }
}

uint16_t LCD_GetWidth(void)
{
    return LCD_Width;
}

uint16_t LCD_GetHeight(void)
{
    return LCD_Height;
}

/*===========================================================================
 *  窗口 / 画点
 *=========================================================================*/
/*********************************************************************
 * @fn      LCD_SetWindow
 * @brief   设置显示窗口, 之后写入的颜色会自动按行填充这个区域
 *          注意: 这里会把逻辑坐标加上显示区偏移 LCD_OffX / LCD_OffY
 *          结束时处于"数据模式"(CS 已拉低), 直接连续写像素即可
 *********************************************************************/
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t gx0 = x0 + LCD_OffX;
    uint16_t gy0 = y0 + LCD_OffY;
    uint16_t gx1 = x1 + LCD_OffX;
    uint16_t gy1 = y1 + LCD_OffY;

    LCD_PORT_BeginCmd();
    LCD_PORT_WriteByte(ST7735_CASET);          /* 列地址 */
    LCD_PORT_BeginData();
    LCD_PORT_WriteByte((uint8_t)(gx0 >> 8));
    LCD_PORT_WriteByte((uint8_t)gx0);
    LCD_PORT_WriteByte((uint8_t)(gx1 >> 8));
    LCD_PORT_WriteByte((uint8_t)gx1);

    LCD_PORT_BeginCmd();
    LCD_PORT_WriteByte(ST7735_RASET);          /* 行地址 */
    LCD_PORT_BeginData();
    LCD_PORT_WriteByte((uint8_t)(gy0 >> 8));
    LCD_PORT_WriteByte((uint8_t)gy0);
    LCD_PORT_WriteByte((uint8_t)(gy1 >> 8));
    LCD_PORT_WriteByte((uint8_t)gy1);

    LCD_PORT_BeginCmd();
    LCD_PORT_WriteByte(ST7735_RAMWR);          /* 准备写显存 */
    LCD_PORT_BeginData();                      /* 接下去直接连续写像素 */
}

/*********************************************************************
 * @fn      LCD_SetCursor
 * @brief   把窗口设成 1x1, 准备写一个点
 *********************************************************************/
void LCD_SetCursor(uint16_t x, uint16_t y)
{
    LCD_SetWindow(x, y, x, y);
}

/*********************************************************************
 * @fn      LCD_WriteColor
 * @brief   在当前窗口里写 1 个像素 (16bit, 高字节在前)
 *********************************************************************/
void LCD_WriteColor(uint16_t color)
{
    LCD_PORT_BeginData();
    LCD_PORT_WriteByte((uint8_t)(color >> 8));
    LCD_PORT_WriteByte((uint8_t)color);
    LCD_PORT_End();
}

/*********************************************************************
 * @fn      LCD_WriteColorFast
 * @brief   在当前窗口里连续写 count 个相同颜色
 *********************************************************************/
void LCD_WriteColorFast(uint16_t color, uint32_t count)
{
    LCD_PORT_BeginData();
    LCD_PORT_WriteRepeat(color, count);
    LCD_PORT_End();
}

/*********************************************************************
 * @fn      LCD_DrawPoint
 * @brief   在 (x,y) 画一个点, 超出屏幕范围自动忽略
 *********************************************************************/
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_Width || y >= LCD_Height)
    {
        return;
    }
    LCD_SetCursor(x, y);
    LCD_WriteColor(color);
}

/*===========================================================================
 *  填充
 *=========================================================================*/
/*********************************************************************
 * @fn      LCD_Fill
 * @brief   用颜色填满一个矩形区域
 *********************************************************************/
void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t total;

    if (w == 0 || h == 0)
    {
        return;
    }
    if (x >= LCD_Width || y >= LCD_Height)
    {
        return;
    }
    if ((uint32_t)x + w > LCD_Width)
    {
        w = LCD_Width - x;
    }
    if ((uint32_t)y + h > LCD_Height)
    {
        h = LCD_Height - y;
    }

    LCD_SetWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    total = (uint32_t)w * h;
    LCD_WriteColorFast(color, total);
}

/*********************************************************************
 * @fn      LCD_Clear
 * @brief   整屏填充
 *********************************************************************/
void LCD_Clear(uint16_t color)
{
    LCD_Fill(0, 0, LCD_Width, LCD_Height, color);
}

/*********************************************************************
 * @fn      LCD_ClearRect
 * @brief   只清掉一块区域(局部刷新的"擦除旧位置"), 比 LCD_Clear() 快得多
 *********************************************************************/
void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    LCD_Fill(x, y, w, h, LCD_BLACK);
}

/*===========================================================================
 *  基本图形
 *=========================================================================*/
void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    if (w == 0 || x >= LCD_Width || y >= LCD_Height)
    {
        return;
    }
    if ((uint32_t)x + w > LCD_Width)
    {
        w = LCD_Width - x;
    }
    LCD_SetWindow(x, y, (uint16_t)(x + w - 1), y);
    LCD_WriteColorFast(color, w);
}

void LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    if (h == 0 || x >= LCD_Width || y >= LCD_Height)
    {
        return;
    }
    if ((uint32_t)y + h > LCD_Height)
    {
        h = LCD_Height - y;
    }
    LCD_SetWindow(x, y, x, (uint16_t)(y + h - 1));
    LCD_WriteColorFast(color, h);
}

/*********************************************************************
 * @fn      LCD_DrawLine
 * @brief   Bresenham 直线算法
 *********************************************************************/
void LCD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1)
    {
        if (x0 >= 0 && y0 >= 0)
        {
            LCD_DrawPoint((uint16_t)x0, (uint16_t)y0, color);
        }
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        {
            int16_t e2 = (int16_t)(err << 1);
            if (e2 > -dy)
            {
                err -= dy;
                x0 = (int16_t)(x0 + sx);
            }
            if (e2 < dx)
            {
                err += dx;
                y0 = (int16_t)(y0 + sy);
            }
        }
    }
}

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (w == 0 || h == 0)
    {
        return;
    }
    LCD_DrawHLine(x, y, w, color);
    LCD_DrawHLine(x, (uint16_t)(y + h - 1), w, color);
    LCD_DrawVLine(x, y, h, color);
    LCD_DrawVLine((uint16_t)(x + w - 1), y, h, color);
}

/*********************************************************************
 * @fn      LCD_DrawCircle
 * @brief   中点画圆算法(只画轮廓)
 *********************************************************************/
void LCD_DrawCircle(uint16_t xc, uint16_t yc, uint16_t r, uint16_t color)
{
    int16_t x = 0;
    int16_t y = (int16_t)r;
    int16_t d = (int16_t)(3 - ((int16_t)r << 1));

    while (x <= y)
    {
        LCD_DrawPoint((uint16_t)(xc + x), (uint16_t)(yc + y), color);
        LCD_DrawPoint((uint16_t)(xc - x), (uint16_t)(yc + y), color);
        LCD_DrawPoint((uint16_t)(xc + x), (uint16_t)(yc - y), color);
        LCD_DrawPoint((uint16_t)(xc - x), (uint16_t)(yc - y), color);
        LCD_DrawPoint((uint16_t)(xc + y), (uint16_t)(yc + x), color);
        LCD_DrawPoint((uint16_t)(xc - y), (uint16_t)(yc + x), color);
        LCD_DrawPoint((uint16_t)(xc + y), (uint16_t)(yc - x), color);
        LCD_DrawPoint((uint16_t)(xc - y), (uint16_t)(yc - x), color);

        if (d < 0)
        {
            d = (int16_t)(d + (x << 2) + 6);
        }
        else
        {
            d = (int16_t)(d + ((x - y) << 2) + 10);
            y--;
        }
        x++;
    }
}

/*===========================================================================
 *  点阵图 / 全彩图 / 精灵
 *=========================================================================*/
/*********************************************************************
 * @fn      LCD_DrawBitmapScaled
 * @brief   画单色点阵图(取模软件输出: 阴码 + 逐行式 + 顺向, 1 表示笔画)
 *********************************************************************/
void LCD_DrawBitmapScaled(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                          uint8_t scale, uint16_t fgColor, uint16_t bgColor)
{
    uint16_t row, col;
    uint16_t rowBytes;
    uint8_t  s;

    if (bmp == NULL || bmp->data == NULL || scale == 0)
    {
        return;
    }

    rowBytes = (uint16_t)((bmp->width + 7u) >> 3);

    for (row = 0; row < bmp->height; row++)
    {
        for (col = 0; col < bmp->width; col++)
        {
            uint8_t  bit   = (uint8_t)((bmp->data[(uint32_t)row * rowBytes + (col >> 3)] >> (7u - (col & 7u))) & 1u);
            uint16_t color = bit ? fgColor : bgColor;

            if (scale == 1)
            {
                LCD_DrawPoint((uint16_t)(x + col), (uint16_t)(y + row), color);
            }
            else
            {
                for (s = 0; s < scale; s++)
                {
                    LCD_DrawHLine((uint16_t)(x + col * scale),
                                  (uint16_t)(y + row * scale + s),
                                  scale, color);
                }
            }
        }
    }
}

void LCD_DrawBitmap(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                    uint16_t fgColor, uint16_t bgColor)
{
    LCD_DrawBitmapScaled(x, y, bmp, 1, fgColor, bgColor);
}

/*********************************************************************
 * @fn      LCD_DrawSpriteEx
 * @brief   画单色点阵精灵(1 画 fgColor; 0 画 bgColor 或跳过)
 *
 *   按行处理: 每行只做 1 次窗口设置, 然后整行数据一次性连续发送。
 *   intoBuffer 非 NULL 时渲染到内存缓冲(每行 bufStride 像素), 不碰屏幕。
 *********************************************************************/
void LCD_DrawSpriteEx(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                      uint16_t fgColor, uint16_t bgColor, uint8_t transparent,
                      uint16_t *intoBuffer, uint16_t bufStride)
{
    uint16_t row, col;
    uint16_t w, h, rowBytes;
    uint16_t fg = fgColor;
    uint16_t bg = bgColor;

    if (bmp == NULL || bmp->data == NULL || bmp->width == 0 || bmp->height == 0)
    {
        return;
    }
    if (bmp->width > LCD_BULK_PIXELS)
    {
        return;                     /* 太宽了, 缓冲区装不下一行 */
    }

    w        = bmp->width;
    h        = bmp->height;
    rowBytes = (uint16_t)((w + 7u) >> 3);

    for (row = 0; row < h; row++)
    {
        const uint8_t *src = &bmp->data[(uint32_t)row * rowBytes];

        /* 先把这一行的颜色算进缓冲 */
        for (col = 0; col < w; col++)
        {
            uint8_t bit = (uint8_t)((src[col >> 3] >> (7u - (col & 7u))) & 1u);
            LCD_BulkBuf[col] = bit ? fg : bg;
        }

        if (intoBuffer != NULL)
        {
            /* 渲染到内存缓冲, 不碰屏幕 */
            uint16_t inRow = (uint16_t)(y + row);
            for (col = 0; col < w; col++)
            {
                uint8_t bit = (uint8_t)((src[col >> 3] >> (7u - (col & 7u))) & 1u);
                if (bit || !transparent)
                {
                    intoBuffer[(uint32_t)inRow * bufStride + x + col] = bit ? fg : bg;
                }
            }
            continue;
        }

        {
            uint16_t py = (uint16_t)(y + row);
            uint16_t pw = w;                /* 允许右边超出屏幕: 只画能显示的部分 */

            if (py >= LCD_Height)
            {
                break;
            }
            if (x >= LCD_Width)
            {
                continue;
            }
            if ((uint32_t)x + pw > LCD_Width)
            {
                pw = (uint16_t)(LCD_Width - x);
            }

            LCD_SetWindow(x, py, (uint16_t)(x + pw - 1), py);
            LCD_PORT_BeginData();
            LCD_PORT_WriteBlock((const uint8_t *)LCD_BulkBuf, pw * 2u);
            LCD_PORT_End();
        }
    }
}

void LCD_DrawSprite(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                    uint16_t fgColor, uint16_t bgColor)
{
    LCD_DrawSpriteEx(x, y, bmp, fgColor, bgColor, 0, NULL, 0);
}

void LCD_DrawSpriteTransparent(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                               uint16_t fgColor)
{
    LCD_DrawSpriteEx(x, y, bmp, fgColor, LCD_BLACK, 1, NULL, 0);
}

/*********************************************************************
 * @fn      LCD_DrawImage
 * @brief   画全彩图片: 数据是 RGB565(高字节在前), 大小 w*h*2 字节
 *********************************************************************/
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565)
{
    if (rgb565 == NULL || w == 0 || h == 0)
    {
        return;
    }
    if (x >= LCD_Width || y >= LCD_Height)
    {
        return;
    }
    if ((uint32_t)x + w > LCD_Width)
    {
        w = LCD_Width - x;
    }
    if ((uint32_t)y + h > LCD_Height)
    {
        h = LCD_Height - y;
    }

    LCD_SetWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    LCD_PORT_BeginData();
    LCD_PORT_WriteBlock(rgb565, (uint32_t)w * h * 2u);
    LCD_PORT_End();
}

/*===========================================================================
 *  显示区偏移自检
 *=========================================================================*/
static void LCD_DrawOffsetFrame(uint16_t offX, uint16_t offY, uint16_t color)
{
    uint16_t w = LCD_Width;
    uint16_t h = LCD_Height;
    uint16_t i;

    LCD_OffX = offX;
    LCD_OffY = offY;

    LCD_Clear(LCD_BLACK);

    /* 外框: 刚好画满逻辑区域。偏移正确时应该正好贴住屏幕四边 */
    LCD_DrawRect(0, 0, w, h, color);
    /* 内框: 往内缩 6 像素, 判断有没有内容被推到屏幕外/绕回来 */
    LCD_DrawRect(6, 6, (uint16_t)(w - 12), (uint16_t)(h - 12), LCD_GRAY);

    /* 上边和左边刻度: 每 16 像素一个点 */
    for (i = 0; i <= w; i += 16)
    {
        LCD_DrawPoint(i, 2, LCD_YELLOW);
        LCD_DrawPoint(i, 3, LCD_YELLOW);
    }
    for (i = 0; i <= h; i += 16)
    {
        LCD_DrawPoint(2, i, LCD_YELLOW);
        LCD_DrawPoint(3, i, LCD_YELLOW);
    }

    /* 四角 6x6 小方块 */
    LCD_Fill(0, 0, 6, 6, LCD_RED);
    LCD_Fill((uint16_t)(w - 6), 0, 6, 6, LCD_GREEN);
    LCD_Fill(0, (uint16_t)(h - 6), 6, 6, LCD_BLUE);
    LCD_Fill((uint16_t)(w - 6), (uint16_t)(h - 6), 6, 6, LCD_WHITE);

    /* 屏幕正中的十字 */
    LCD_DrawHLine((uint16_t)(w / 2 - 10), (uint16_t)(h / 2), 21, color);
    LCD_DrawVLine((uint16_t)(w / 2), (uint16_t)(h / 2 - 10), 21, color);

    /* 顶部彩色条: 第一行红块个数 = offX, 第二行绿块个数 = offY */
    LCD_Fill(8, 10, 2, 8, LCD_WHITE);
    for (i = 0; i < offX && i < 40; i++)
    {
        LCD_Fill((uint16_t)(11 + i * 5), 10, 4, 8, LCD_RED);
    }
    LCD_Fill(11, 20, 2, 8, LCD_WHITE);
    for (i = 0; i < offY && i < 40; i++)
    {
        LCD_Fill((uint16_t)(14 + i * 5), 20, 4, 8, LCD_GREEN);
    }
}

/*********************************************************************
 * @fn      LCD_OffsetTest
 * @brief   显示区偏移自检: 循环切换 4 组偏移值, 每组显示 2 秒
 *********************************************************************/
void LCD_OffsetTest(void)
{
    static const uint8_t tbl[4][2] = {{2, 1}, {2, 0}, {0, 1}, {0, 0}};
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
        LCD_DrawOffsetFrame(tbl[i][0], tbl[i][1], LCD_CYAN);
        Delay_Ms(2000);
    }

    LCD_DrawOffsetFrame(LCD_X_OFFSET, LCD_Y_OFFSET, LCD_CYAN);
    Delay_Ms(300);
    LCD_Clear(LCD_BLACK);
}

/*===========================================================================
 *  兼容老工程的名字
 *=========================================================================*/
void Lcd_WriteData_16Bit(uint16_t Data)
{
    LCD_WriteColor(Data);
}

void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd)
{
    LCD_SetWindow(xStar, yStar, xEnd, yEnd);
}

void LCD_direction(uint8_t direction)
{
    LCD_Direction(direction);
}

/*********************************************************************
 * @fn      LCD_drawImageWithSize
 * @brief   兼容 WCH 老例程的名字(从 SPI Flash 读图那套已不再使用)
 *********************************************************************/
void LCD_drawImageWithSize(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t addr)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)addr;
}
