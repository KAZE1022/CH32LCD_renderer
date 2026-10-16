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

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("SystemClk:%u\r\n", (unsigned int)SystemCoreClock);

    LCD_Init();
    Delay_Ms(300);
    init_backgroundcolor(C_HEX_00FF00);
    fullblock(0,0,W,H/3,C_R,255);
    fullblock(0,H/3*1,W,H/3,C_G,255);
    fullblock(0,H/3*2,W,H/3,C_B,255);
    while (1)
    {       
        flush();
        Delay_Ms(33);
    }
}