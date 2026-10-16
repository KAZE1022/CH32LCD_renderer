/*********************************************************************
 * File Name          : lcd.h
 * Description        : ST7735S (1.8寸 128x160) 驱动
 *
 *   接线(见 config.h):
 *      SCL = PB13(SPI2_SCK)   SDA = PB15(SPI2_MOSI)
 *      CS  = PB12             DC  = PB14
 *      RST = PC4              BLK = PC5
 *   颜色:  RGB565, 例如 红色 = 0xF800
 *
 *   底层由 lcd_port.c 负责: 硬件 SPI2 和软件模拟 SPI 用 config.h 里的
 *   LCD_USE_HARD_SPI 切换, 上层代码不用改。
 *
 *   只依赖: User\main.c  LCD\config.h  LCD\lcd_port.c/h  LCD\lcd.c/h
 *******************************************************************************/
#ifndef __LCD_H
#define __LCD_H

#include "debug.h"
#include "config.h"
#include "lcd_port.h"

/*---------------------------------------------------------------------------
 * 常用颜色 (RGB565)
 *-------------------------------------------------------------------------*/
#define LCD_WHITE        0xFFFF
#define LCD_BLACK        0x0000
#define LCD_BLUE         0x001F
#define LCD_RED          0xF800
#define LCD_GREEN        0x07E0
#define LCD_CYAN         0x07FF
#define LCD_MAGENTA      0xF81F
#define LCD_YELLOW       0xFFE0
#define LCD_GRAY         0x8410
#define LCD_ORANGE       0xFD20
#define LCD_PURPLE       0x780F

/* 24 位 RGB888 -> RGB565, 例如 LCD_RGB888(255,0,0) 就是红色 */
#define LCD_RGB888(r, g, b)  ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

/*---------------------------------------------------------------------------
 * 单色点阵图 (取模软件输出: 阴码 / 逐行式 / 顺向, 1bit/像素, 高位在左)
 *   这是后面"取字模画字"要用的结构, 现在先用它画简单的图形
 *-------------------------------------------------------------------------*/
typedef struct
{
    uint16_t       width;    /* 宽度(像素)                          */
    uint16_t       height;   /* 高度(像素)                          */
    const uint8_t *data;     /* 数据, 每行 (width+7)/8 字节         */
} LCD_Bitmap;

/*---------------------------------------------------------------------------
 * 初始化 / 基础控制
 *-------------------------------------------------------------------------*/
void     LCD_Init(void);                    /* 初始化 + 清屏 + 点亮背光 */
void     LCD_Reset(void);                   /* 硬复位 */
void     LCD_BackLight(uint8_t on);         /* 1=亮 0=灭 */
void     LCD_DisplayOn(void);
void     LCD_DisplayOff(void);
void     LCD_Direction(uint8_t direction);  /* 0=竖屏 1=横屏 2=竖屏翻转 3=横屏翻转 */
uint16_t LCD_GetWidth(void);
uint16_t LCD_GetHeight(void);

/* 显示区偏移自检: 循环显示 4 组偏移值, 每组 2 秒。
 * 看到"红框正好贴住屏幕四边、四角小方块都在角上"时, 那一组的 (X,Y) 就是正确偏移,
 * 填回 config.h 的 LCD_X_OFFSET / LCD_Y_OFFSET 即可。
 * 屏幕上另外用彩色小方块把当前偏移值显示出来(不用字库):
 *    第一行: 白竖线后面 1 个红块 = X_OFF 是 1, 2 个红块 = X_OFF 是 2
 *    第二行: 绿块个数 = Y_OFF  */
void     LCD_OffsetTest(void);

/*---------------------------------------------------------------------------
 * 画点相关 (最基本的功能)
 *-------------------------------------------------------------------------*/
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_SetCursor(uint16_t x, uint16_t y);
void LCD_WriteColor(uint16_t color);                     /* 当前窗口内写 1 点 */
void LCD_WriteColorFast(uint16_t color, uint32_t count); /* 当前窗口内写 n 点 */

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);   /* 画一个点 */

/*---------------------------------------------------------------------------
 * 填充 / 清屏
 *-------------------------------------------------------------------------*/
void LCD_Clear(uint16_t color);                                          /* 整屏填充 */
void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);      /* 局部擦除(清成黑色) */

/*---------------------------------------------------------------------------
 * 基本图形 (都由画点实现)
 *-------------------------------------------------------------------------*/
void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void LCD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawCircle(uint16_t xc, uint16_t yc, uint16_t r, uint16_t color);

/*---------------------------------------------------------------------------
 * 画点阵图 / 彩色图
 *-------------------------------------------------------------------------*/
/* 单色点阵图: 1 画 fgColor, 0 画 bgColor; scale 为放大倍数(1/2/3...) */
void LCD_DrawBitmapScaled(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                          uint8_t scale, uint16_t fgColor, uint16_t bgColor);
void LCD_DrawBitmap(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                    uint16_t fgColor, uint16_t bgColor);

/* 单色点阵精灵: 按行整行连续发送, 比逐点画快几十倍(做局部刷新/移动图形用) */
void LCD_DrawSprite(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                    uint16_t fgColor, uint16_t bgColor);
/* 只画笔画, 0 的位置保留屏幕原内容(叠在图片/画面上用) */
void LCD_DrawSpriteTransparent(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                               uint16_t fgColor);
/* 通用版本: 可以直接渲染到内存里的 RGB565 缓冲(不碰屏幕), 用来拼整块画面 */
void LCD_DrawSpriteEx(uint16_t x, uint16_t y, const LCD_Bitmap *bmp,
                      uint16_t fgColor, uint16_t bgColor, uint8_t transparent,
                      uint16_t *intoBuffer, uint16_t bufStride);

/* 全彩位图: 数据为 RGB565, 高字节在前(大端) */
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *rgb565);

/*---------------------------------------------------------------------------
 * 性能实测(用串口打印耗时), 用来对比局部刷新和全屏刷新的差距
 *-------------------------------------------------------------------------*/
void LCD_Benchmark(void);

/*---------------------------------------------------------------------------
 * 低层命令读写 (调试用)
 *-------------------------------------------------------------------------*/
void LCD_WriteCommand(uint8_t cmd);
void LCD_WriteData8(uint8_t data);
void LCD_WriteCommandData(uint8_t cmd, const uint8_t *data, uint32_t len);

#endif /* __LCD_H */
