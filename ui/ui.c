#include "ui.h"


uint16_t fb[W * H];

static uint16_t line_swap[W];   /* 新增一行：字节交换用的小缓冲 */




void flush(void)
{
    LCD_SetWindow(0, 0, W - 1, H - 1);
    LCD_PORT_BeginData();

    for (uint16_t y = 0; y < H; y++) {
        const uint16_t *src = fb + (uint32_t)y * W;
        for (uint16_t x = 0; x < W; x++)
            line_swap[x] = (uint16_t)((src[x] >> 8) | (src[x] << 8));  /* ← 交换高低字节 */
        LCD_PORT_WriteBlock((const uint8_t *)line_swap, (uint32_t)W * 2u);
    }

    LCD_PORT_End();
}

/* ---------------- 透明度混合核心 ---------------- */
uint16_t blend_alpha(uint16_t bg, uint16_t fg, uint8_t alpha)
{
    if (alpha >= 255) return fg;
    if (alpha == 0)   return bg;

    uint32_t ia = 255 - alpha;
    uint32_t br = (bg >> 11) & 0x1F, bg6 = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    uint32_t fr = (fg >> 11) & 0x1F, fg6 = (fg >> 5) & 0x3F, fb_ = fg & 0x1F;

    uint32_t r = (br  * ia + fr  * alpha) / 255;
    uint32_t g = (bg6 * ia + fg6 * alpha) / 255;
    uint32_t b = (bb  * ia + fb_ * alpha) / 255;

    return (uint16_t)((r << 11) | (g << 5) | b);
}

/*
 *  在 (x, y) 处，用 color 填一个 weight × height 的矩形
 *  ops = 透明度(0=全透, 255=不透明)
 */
void fullblock(uint16_t x, uint16_t y, uint16_t weight, uint16_t height,
               uint16_t color, uint8_t ops)
{
    /* 1) 边界裁剪 */
    if (x >= W || y >= H) return;
    if ((uint32_t)x + weight > W) weight = W - x;
    if ((uint32_t)y + height > H) height = H - y;

    /* 2) 遍历矩形里的每个像素 */
    for (uint16_t yy = 0; yy < height; yy++) {
        /* 一行一行算，效率比每次乘 y*W 高 */
        uint16_t *row = fb + (uint32_t)(y + yy) * W + x;

        for (uint16_t xx = 0; xx < weight; xx++) {
            if (ops >= 255) {
                row[xx] = color;                    /* 不透明：直接覆盖 */
            } else if (ops > 0 && ops < 255) {
                row[xx] = blend_alpha(row[xx], color, ops);   /* ← 核心 */
            }
            /* ops == 0 就什么都不做，透出背景 */
        }
    }
}


/* 两个颜色各占一半 */
uint16_t blend2(uint16_t a, uint16_t b)
{
    uint16_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint16_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;

    uint16_t r = (ar + br) / 2;
    uint16_t g = (ag + bg) / 2;
    uint16_t bl = (ab + bb) / 2;

    return (r << 11) | (g << 5) | bl;
}
#define err 0
/*
 *  this function to set single point color and fix tow color
 *
 */

void set_pixcolor(uint16_t *fb, uint16_t color, uint16_t x, uint16_t y){
    if(x >= W || y >= H) return;
    uint16_t *p = fb + (uint32_t)y*W + x;       //取出当前的数组里面的颜色                          

    *p = blend2(*p,color);                      //取出颜色和目标颜色50%混合 得出 混合后颜色 返回给指针指向的数组里
}

void init_backgroundcolor(uint16_t color)
{
    for (int i = 0; i < W * H; i++) fb[i] = color;
}


