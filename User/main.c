/*********************************************************************
 * main.c  —  ST7735S + 自研 UI 图层叠加渲染（单文件版）
 *
 *   图层从底到顶依次合成到一行像素缓冲，再整行扔给 LCD。
 *   支持：纯色层 / 位图层 / 色键透明 / 全局 alpha 混合 / 脏矩形局部刷
 *********************************************************************/
#include "debug.h"
#include "lcd.h"
#include "lcd_port.h"
#include "fps.h"
#include "../ui/ui.h"
#include "../ui/color_config.h"

/* ================================================================
 *  带透明度的单点
 * ================================================================ */
static void put_pixel_a(int16_t x, int16_t y, uint16_t color, uint8_t alpha)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return;

    uint16_t *p = fb + (uint32_t)y * W + x;

    if (alpha >= 255)     *p = color;
    else if (alpha > 0)   *p = blend_alpha(*p, color, alpha);
}

/* ================================================================
 *  整数平方根（逐次逼近）
 * ================================================================ */
static int sqrt_int(int v)
{
    int x = 0;
    if (v <= 0) return 0;
    while ((x + 1) * (x + 1) <= v) x++;
    return x;
}

/* ================================================================
 *  1/4 圆弧（中点画圆算法）
 *    cx, cy : 圆心
 *    r      : 半径
 *    sx, sy : ±1，决定画哪个象限
 * ================================================================ */
static void draw_arc_quadrant(int16_t cx, int16_t cy, int16_t r,
                              int sx, int sy, uint16_t color, uint8_t alpha)
{
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;

    while (x <= y) {
        put_pixel_a(cx + sx * x, cy + sy * y, color, alpha);
        put_pixel_a(cx + sx * y, cy + sy * x, color, alpha);

        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/* ================================================================
 *  圆角矩形边框
 * ================================================================ */
void draw_round_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                     int16_t r, uint16_t color, uint8_t alpha)
{
    if (w < 2 || h < 2) return;

    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    /* r = 0 退化为普通矩形 */
    if (r <= 0) {
        for (int16_t i = 0; i < w; i++) {
            put_pixel_a(x + i, y,         color, alpha);
            put_pixel_a(x + i, y + h - 1, color, alpha);
        }
        for (int16_t i = 0; i < h; i++) {
            put_pixel_a(x,         y + i, color, alpha);
            put_pixel_a(x + w - 1, y + i, color, alpha);
        }
        return;
    }

    /* 四条直边 */
    for (int16_t i = x + r; i <= x + w - 1 - r; i++) {
        put_pixel_a(i, y,         color, alpha);
        put_pixel_a(i, y + h - 1, color, alpha);
    }
    for (int16_t i = y + r; i <= y + h - 1 - r; i++) {
        put_pixel_a(x,         i, color, alpha);
        put_pixel_a(x + w - 1, i, color, alpha);
    }

    /* 四个角的圆弧 */
    int16_t cx_l = x + r;
    int16_t cx_r = x + w - 1 - r;
    int16_t cy_t = y + r;
    int16_t cy_b = y + h - 1 - r;

    draw_arc_quadrant(cx_l, cy_t, r, -1, -1, color, alpha);
    draw_arc_quadrant(cx_r, cy_t, r,  1, -1, color, alpha);
    draw_arc_quadrant(cx_l, cy_b, r, -1,  1, color, alpha);
    draw_arc_quadrant(cx_r, cy_b, r,  1,  1, color, alpha);
}

/* ================================================================
 *  圆角矩形填充
 * ================================================================ */
void fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                     int16_t r, uint16_t color, uint8_t alpha)
{
    if (w <= 0 || h <= 0) return;

    if (r <= 0) {
        for (int16_t yy = 0; yy < h; yy++)
            for (int16_t xx = 0; xx < w; xx++)
                put_pixel_a(x + xx, y + yy, color, alpha);
        return;
    }

    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    int rr = r * r;

    for (int16_t yy = 0; yy < h; yy++) {
        int16_t inset = 0;

        if (yy < r) {
            int dy = r - 1 - yy;
            inset = r - sqrt_int(rr - dy * dy);
        } else if (yy >= h - r) {
            int dy = r - 1 - (h - 1 - yy);
            inset = r - sqrt_int(rr - dy * dy);
        }

        int16_t sx = x + inset;
        int16_t ex = x + w - 1 - inset;
        for (int16_t xx = sx; xx <= ex; xx++)
            put_pixel_a(xx, y + yy, color, alpha);
    }
}

void create_button(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    uint16_t r, uint16_t color, uint8_t alpha){
    fill_round_rect(x,y,w,h,r,color,alpha);
}

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("SystemClk:%u\r\n", (unsigned int)SystemCoreClock);

    LCD_Init();
    Delay_Ms(300);
    init_backgroundcolor(0xffff);
    fill_round_rect(W/10,H/12,100,80,10,0x0000,20);
    create_button(10,10,100,50,15,0x1212,25);
    while (1)
    {       
        flush();
        Delay_Ms(33);
    }
}











