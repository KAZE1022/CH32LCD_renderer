/*********************************************************************
 * File Name          : font.h
 * Description        : 字库模块 (ASCII 8x16  +  中文 16x16 HZK16)
 *
 *   点阵格式: 逐行, 高位在左 (和常见的 PCtoLCD2002 "阴码/逐行式/顺向" 一致)
 *
 *   中文: 内置 261696 字节的 HZK16(GB2312) 字库 (font_hz16.c)
 *         常用汉字 6763 个全部包含, 每个 32 字节:
 *             地址 = (字符索引) * 32
 *             字符索引 = (区码-0xA1)*94 + (位码-0xA1)
 *   英文: 内置 8x16 点阵 (font_asc.c), 覆盖 0x20~0x7E
 *
 *   中文显示时源文件用 UTF-8 或 ANSI(GB2312) 都可以, 驱动会自动识别。
 *******************************************************************************/
#ifndef __FONT_H__
#define __FONT_H__

#include "debug.h"

/*---------------------------------------------------------------------------
 * 点阵字形
 *-------------------------------------------------------------------------*/
typedef struct
{
    uint8_t  width;      /* 字宽: 8 或 16           */
    uint8_t  height;     /* 字高: 16                */
    uint8_t  rowBytes;   /* 每行字节数: width/8     */
    uint8_t  isHzk;      /* 1 = 中文(HZK16 格式, 每行 2 字节) */
    uint8_t  rows[32];   /* 点阵数据(统一成逐行/高位在左) */
} Font_Glyph;

/*---------------------------------------------------------------------------
 * 字库数据 (由 tools 目录下的脚本生成, 见 README.md)
 *-------------------------------------------------------------------------*/
extern const unsigned char ASC8x16_FontData[1520];       /* 0x20~0x7E, 16 字节/字 */
extern const unsigned char HZK16_FontData[261696];       /* GB2312 全字库, 32 字节/字 */

/* Unicode <-> 字库 映射表 (font_gb2312_map.c) */
extern const uint16_t Font_HZK_Unicode[];
extern const uint16_t Font_HZK_SortedUni[];
extern const uint16_t Font_HZK_SortedMap[];
extern const uint16_t Font_HZK_SortedGroup[17];
extern const uint32_t Font_HZK_FirstCharIndex;
extern const uint32_t Font_HZK_CharCount;

/*---------------------------------------------------------------------------
 * 字符串解码: 兼容 UTF-8 与 GB2312/ANSI 两种源文件编码
 *-------------------------------------------------------------------------*/
/* 从 str 指向的位置解出一个字符。
 * 返回值:
 *      1 = 单字节 ASCII,    *code 为 0x00~0xFF
 *      2 = 双字节中文/符号, *code 为 GB2312 编码(高字节<<8 | 低字节)
 *      3 = 解码得到的 UTF-8 字符不在 GB2312 字库里(画空白格), *code 无意义
 *      -1 = 非法序列结束(调用方步进 1 字节)
 */
int8_t Font_DecodeChar(const char *str, uint16_t *code);

/*---------------------------------------------------------------------------
 * 取字模
 *-------------------------------------------------------------------------*/
/* 取 8x16 英文字模, 返回 0 成功。code 为 ASCII 值 (0x20~0x7E) */
int8_t Font_GetAsciiGlyph(uint16_t code, Font_Glyph *glyph);

/* 取 16x16 中文字模, 返回 0 成功。
 * code 为 GB2312 编码(如 "啊" = 0xB0A1); UTF-8 解码得到的字符会先查表转换 */
int8_t Font_GetHzkGlyph(uint16_t code, Font_Glyph *glyph);

/* 通用取模: 自动判断 ASCII / 中文, 返回 0 成功 */
int8_t Font_GetGlyph(uint16_t code, Font_Glyph *glyph);

/* 取字库中某个索引的字形 (0 = 啊 0xB0A1) */
int8_t Font_GetGlyphByIndex(uint16_t index, Font_Glyph *glyph);

/* 把点阵的某一位展开成 0/1, 供画图用 (返回 1 表示该点有笔画) */
#define FONT_GLYPH_BIT(g, row, col) \
    ((((g)->rows[(row) * (g)->rowBytes + ((col) >> 3)]) >> (7 - ((col) & 7))) & 0x01)

#endif /* __FONT_H__ */
