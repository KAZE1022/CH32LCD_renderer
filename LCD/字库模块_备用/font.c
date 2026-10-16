/*********************************************************************
 * File Name          : font.c
 * Description        : 字库模块实现 (ASCII 8x16 + 中文 16x16 HZK16)
 *******************************************************************************/

#include "font.h"
#include "string.h"

/* HZK16 里第一个字符(0xA1A1)对应的索引 */
#define HZK_INDEX(ch)   ((((uint32_t)((ch) >> 8) - 0xA1u) * 94u) + (((ch) & 0xFFu) - 0xA1u))

/* 每个中文字符的字形字节数 */
#define HZK_GLYPH_BYTES 32u

/*===========================================================================
 *  内部: UTF-8 -> Unicode
 *=========================================================================*/
static int Utf8Decode(const uint8_t *s, uint32_t *unicode)
{
    uint8_t c = s[0];

    if (c < 0x80u)
    {
        *unicode = c;
        return 1;
    }
    if ((c & 0xE0u) == 0xC0u)
    {
        if ((s[1] & 0xC0u) != 0x80u) { return -1; }
        *unicode = ((uint32_t)(c & 0x1Fu) << 6) | (uint32_t)(s[1] & 0x3Fu);
        return 2;
    }
    if ((c & 0xF0u) == 0xE0u)
    {
        if ((s[1] & 0xC0u) != 0x80u || (s[2] & 0xC0u) != 0x80u) { return -1; }
        *unicode = ((uint32_t)(c & 0x0Fu) << 12) |
                   ((uint32_t)(s[1] & 0x3Fu) << 6) |
                   (uint32_t)(s[2] & 0x3Fu);
        return 3;
    }
    if ((c & 0xF8u) == 0xF0u)
    {
        if ((s[1] & 0xC0u) != 0x80u || (s[2] & 0xC0u) != 0x80u || (s[3] & 0xC0u) != 0x80u) { return -1; }
        *unicode = ((uint32_t)(c & 0x07u) << 18) |
                   ((uint32_t)(s[1] & 0x3Fu) << 12) |
                   ((uint32_t)(s[2] & 0x3Fu) << 6) |
                   (uint32_t)(s[3] & 0x3Fu);
        return 4;
    }
    return -1;
}

/*===========================================================================
 *  内部: Unicode(汉字) -> HZK16 字符索引 (0 = 啊)
 *  用 Font_HZK_SortedGroup 分区 + 二分查找, 失败返回 -1
 *=========================================================================*/
static int32_t UnicodeToHzkIndex(uint32_t unicode)
{
    if (unicode < 0x4E00u || unicode > 0x9FA5u)
    {
        return -1;
    }

    uint32_t group = unicode >> 12;                 /* 0x4 ~ 0x9 */
    if (group > 15u)
    {
        return -1;
    }

    uint32_t lo = Font_HZK_SortedGroup[group];
    uint32_t hi = Font_HZK_SortedGroup[group + 1];

    if (hi <= lo || hi > Font_HZK_CharCount)
    {
        return -1;
    }
    hi--;

    while (lo <= hi)
    {
        uint32_t mid = (lo + hi) >> 1;
        uint16_t v   = Font_HZK_SortedUni[mid];
        if ((uint32_t)v == unicode)
        {
            return (int32_t)Font_HZK_SortedMap[mid];
        }
        if ((uint32_t)v < unicode)
        {
            lo = mid + 1;
        }
        else
        {
            if (mid == 0) { break; }
            hi = mid - 1;
        }
    }
    return -1;
}

/*===========================================================================
 *  内部: GB2312 合法字符判断(只认 0xA1~0xFE 的双字节, 覆盖 GB2312 全集)
 *=========================================================================*/
static int IsGb2312(uint16_t code)
{
    uint8_t hi = (uint8_t)(code >> 8);
    uint8_t lo = (uint8_t)(code & 0xFFu);
    return (hi >= 0xA1u && hi <= 0xF7u && lo >= 0xA1u && lo <= 0xFEu);
}

/*===========================================================================
 *  Font_DecodeChar
 *=========================================================================*/
int8_t Font_DecodeChar(const char *str, uint16_t *code)
{
    const uint8_t *s = (const uint8_t *)str;
    uint32_t uni   = 0;
    int      n     = 0;

    if (str == NULL || s[0] == 0)
    {
        return -1;
    }

    *code = 0;

    /* --- 1) 先按 UTF-8 试 --- */
    n = Utf8Decode(s, &uni);
    if (n > 0)
    {
        if (n == 1)
        {
            *code = (uint16_t)uni;                  /* ASCII */
            return 1;
        }

        /* 非 ASCII: 2/3/4 字节的 UTF-8 序列只有在"整串都像 UTF-8"时才成立,
           这里加了宽松校验, 目的是与 GB2312 双字节区分开:
           GB2312 高字节 0xA1~0xF7 一定 >= 0xC2, 且第二字节 0xA1~0xFE >= 0x80,
           所以单个 GB 汉字也会被当成"合法 UTF-8 两字节序列"。
           解决: 用 Unicode 码点范围判断 -- GB2312 汉字对应的 Unicode 在 0x4E00~0x9FA5,
           若解出来不在中文/常见符号区, 就回退按 GB2312 处理。 */
        if (n == 2)
        {
            /* 0x4E00~0x9FA5 是汉字区, 落在里面就认为确实是 UTF-8 */
            if (uni >= 0x4E00u && uni <= 0x9FA5u)
            {
                int32_t idx = UnicodeToHzkIndex(uni);
                if (idx >= 0)
                {
                    /* 把 UTF-8 解码结果换算成该字的 GB2312 编码, 供画字用 */
                    uint32_t chIndex = Font_HZK_FirstCharIndex + (uint32_t)idx;
                    uint32_t zone    = 0xA1u + (chIndex / 94u);
                    uint32_t pos     = 0xA1u + (chIndex % 94u);
                    *code = (uint16_t)((zone << 8) | pos);
                    return 2;
                }
                return 3;                            /* UTF-8 汉字但字库没有 */
            }
            /* 其它 UTF-8 两字节(拉丁扩展等): 按 GB2312 再判一次 */
            if (IsGb2312((uint16_t)((s[0] << 8) | s[1])))
            {
                *code = (uint16_t)((s[0] << 8) | s[1]);   /* GB2312 源文件 */
                return 2;
            }
            return 3;                                       /* 字库没有的字符 */
        }

        /* 3/4 字节 UTF-8: 常见于全角标点、生僻字, 统一走 Unicode 查表 */
        {
            int32_t idx = UnicodeToHzkIndex(uni);
            if (idx >= 0)
            {
                uint32_t chIndex = Font_HZK_FirstCharIndex + (uint32_t)idx;
                uint32_t zone    = 0xA1u + (chIndex / 94u);
                uint32_t pos     = 0xA1u + (chIndex % 94u);
                *code = (uint16_t)((zone << 8) | pos);
                return 2;
            }
        }
        return 3;
    }

    /* --- 2) UTF-8 解码失败, 按 GB2312 / ANSI 处理 --- */
    if (s[0] >= 0xA1u && s[1] != 0)
    {
        uint16_t gb = (uint16_t)((s[0] << 8) | s[1]);
        if (IsGb2312(gb))
        {
            *code = gb;
            return 2;
        }
    }

    *code = s[0];
    return 1;
}

/*===========================================================================
 *  取 8x16 英文字模
 *=========================================================================*/
int8_t Font_GetAsciiGlyph(uint16_t code, Font_Glyph *glyph)
{
    if (glyph == NULL)
    {
        return -1;
    }
    if (code < 0x20u || code > 0x7Eu)
    {
        return -1;
    }

    glyph->width    = 8;
    glyph->height   = 16;
    glyph->rowBytes = 1;
    glyph->isHzk    = 0;

    memcpy(glyph->rows, &ASC8x16_FontData[(code - 0x20u) * 16u], 16);
    return 0;
}

/*===========================================================================
 *  取 16x16 中文字模
 *=========================================================================*/
int8_t Font_GetHzkGlyph(uint16_t code, Font_Glyph *glyph)
{
    if (glyph == NULL)
    {
        return -1;
    }

    uint32_t index = HZK_INDEX(code);
    if (index >= (sizeof(HZK16_FontData) / HZK_GLYPH_BYTES))
    {
        return -1;
    }

    glyph->width    = 16;
    glyph->height   = 16;
    glyph->rowBytes = 2;
    glyph->isHzk    = 1;

    memcpy(glyph->rows, &HZK16_FontData[index * HZK_GLYPH_BYTES], HZK_GLYPH_BYTES);
    return 0;
}

/*===========================================================================
 *  按字库索引取字形
 *=========================================================================*/
int8_t Font_GetGlyphByIndex(uint16_t index, Font_Glyph *glyph)
{
    if (glyph == NULL)
    {
        return -1;
    }
    if ((uint32_t)index >= (sizeof(HZK16_FontData) / HZK_GLYPH_BYTES))
    {
        return -1;
    }

    glyph->width    = 16;
    glyph->height   = 16;
    glyph->rowBytes = 2;
    glyph->isHzk    = 1;

    memcpy(glyph->rows, &HZK16_FontData[(uint32_t)index * HZK_GLYPH_BYTES], HZK_GLYPH_BYTES);
    return 0;
}

/*===========================================================================
 *  通用取模
 *=========================================================================*/
int8_t Font_GetGlyph(uint16_t code, Font_Glyph *glyph)
{
    if (glyph == NULL)
    {
        return -1;
    }
    if (code < 0x80u)
    {
        return Font_GetAsciiGlyph(code, glyph);
    }
    return Font_GetHzkGlyph(code, glyph);
}
