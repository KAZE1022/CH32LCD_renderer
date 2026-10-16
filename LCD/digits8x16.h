/*********************************************************************
 * File Name          : digits8x16.h
 * Description        : 8x16 数字/字母点阵, 给 FPS 显示之类的界面小部件用
 *
 *   由 tools/gen_digits8x16.ps1 自动生成 (LCD/digits8x16.c)
 *   覆盖 0x20(' ') ~ 0x5A('Z'), 每字 16 字节, 逐行, 高位在左, 1 = 有笔画
 *******************************************************************************/
#ifndef __DIGITS8X16_H__
#define __DIGITS8X16_H__

#include "debug.h"

#define DIGITS8X16_FIRST   0x20u        /* ' ' */
#define DIGITS8X16_LAST    0x5Au        /* 'Z' */
#define DIGITS8X16_COUNT   (DIGITS8X16_LAST - DIGITS8X16_FIRST + 1u)
#define DIGITS8X16_WIDTH   8u
#define DIGITS8X16_HEIGHT  16u

extern const unsigned char Digits8x16[DIGITS8X16_COUNT * 16u];

/* 取某个字符的点阵指针(字符不在范围内时返回 '?' 的点阵) */
#define DIGITS8X16_PTR(ch) \
    (&Digits8x16[((((ch) >= DIGITS8X16_FIRST) && ((ch) <= DIGITS8X16_LAST)) \
                  ? ((ch) - DIGITS8X16_FIRST) : (0x3Fu - DIGITS8X16_FIRST)) * 16u])

#endif /* __DIGITS8X16_H__ */
