#ifndef __color_config_h
#define __color_config_h

/* ================================================================
 *  颜色宏：RGB565 (r5 g6 b5)
 *
 *  两种写法：
 *    A. 从 0~255 的 R/G/B 分量生成 → 直观，适合设计色
 *    B. 直接给 16 位十六进制       → 快，适合已知值
 * ================================================================ */
#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8u)<<8) | (((g)&0xFCu)<<3) | ((b)>>3)))

/* ================================================================
 *  1. 基础色（16 种最常用）
 * ================================================================ */
#define C_BLACK       0x0000
#define C_WHITE       0xFFFF
#define C_RED         0xF800
#define C_GREEN       0x07E0
#define C_BLUE        0x001F
#define C_YELLOW      0xFFE0
#define C_CYAN        0x07FF
#define C_MAGENTA     0xF81F
#define C_GRAY        0x8410   /* 50% 灰 */
#define C_ORANGE      0xFD20
#define C_PURPLE      0x8010
#define C_PINK        0xFC1F
#define C_BROWN       0xA145
#define C_GOLD        0xFEA0
#define C_SILVER      0xC618
#define C_NAVY        0x000F

/* ================================================================
 *  2. 灰度等级（8 档，从黑到白）
 * ================================================================ */
#define C_GRAY_0      0x0000   /* 黑 */
#define C_GRAY_1      0x2104
#define C_GRAY_2      0x4208
#define C_GRAY_3      0x630C
#define C_GRAY_4      0x8410   /* 中灰 */
#define C_GRAY_5      0xA514
#define C_GRAY_6      0xC618
#define C_GRAY_7      0xE71C
#define C_GRAY_8      0xFFFF   /* 白 */

/* ================================================================
 *  3. 暗色主题 UI 配色（现代 App 风格）
 * ================================================================ */
/* --- 背景 --- */
#define C_BG_DARK     0x0841   /* #080810 接近黑 */
#define C_BG          0x10A2   /* #101010 */
#define C_BG_LIGHT    0x2124   /* #202030 */

/* --- 面板 / 卡片 --- */
#define C_PANEL       0x18E3   /* 深灰面板 */
#define C_PANEL_HI    0x2945   /* 面板高光 */
#define C_BORDER      0x39E7   /* 边框灰 */

/* --- 文字 --- */
#define C_TEXT        0xEF5D   /* 主文字（近白） */
#define C_TEXT_DIM    0x8410   /* 次要文字（中灰） */
#define C_TEXT_HINT   0x528A   /* 提示文字 */

/* --- 主题强调色 --- */
#define C_ACCENT      0x2D9F   /* 蓝色强调 */
#define C_ACCENT_DIM  0x1C4F   /* 蓝色暗版 */
#define C_SUCCESS     0x2E69   /* 绿色成功 */
#define C_WARNING     0xFE40   /* 橙色警告 */
#define C_DANGER      0xD985   /* 红色危险 */
#define C_INFO        0x4D3F   /* 青蓝信息 */

/* ================================================================
 *  4. 亮色主题 UI 配色
 * ================================================================ */
#define C_LIGHT_BG    0xFFFF
#define C_LIGHT_PANEL 0xF7BE
#define C_LIGHT_TEXT  0x0000
#define C_LIGHT_DIM   0x8410

/* ================================================================
 *  5. 状态 / 指示灯色
 * ================================================================ */
#define C_LED_ON      0x07E0   /* 亮绿 */
#define C_LED_OFF     0x39E7   /* 暗灰 */
#define C_LED_WARN    0xFEA0   /* 黄 */
#define C_LED_ERR     0xF800   /* 红 */
#define C_LED_BLUE    0x001F   /* 蓝 */

/* ================================================================
 *  6. 常用设计色（精确 RGB → RGB565）
 * ================================================================ */
#define C_CRIMSON     0xE0C5   /* RGB(220, 20, 60)  深红 */
#define C_TOMATO      0xFB4C   /* RGB(255, 99, 71)  番茄红 */
#define C_CORAL       0xFC0F   /* RGB(255, 127, 80) 珊瑚 */
#define C_SALMON      0xFC6D   /* RGB(250, 128, 114) */

#define C_LIME        0x07E0   /* RGB(0, 255, 0)    亮绿 */
#define C_FOREST      0x2444   /* RGB(34, 139, 34)  森林绿 */
#define C_OLIVE       0x8400   /* RGB(128, 128, 0)  橄榄 */
#define C_MINT        0x9FF3   /* RGB(152, 255, 152) 薄荷 */

#define C_SKY         0x867D   /* RGB(135, 206, 235) 天蓝 */
#define C_STEEL       0x4416   /* RGB(70, 130, 180)  钢蓝 */
#define C_ROYAL       0x435C   /* RGB(65, 105, 225)  皇室蓝 */
#define C_INDIGO      0x4A29   /* RGB(75, 0, 130)    靛蓝 */

#define C_VIOLET      0xEC1D   /* RGB(238, 130, 238) 紫罗兰 */
#define C_ORCHID      0xDB9A   /* RGB(218, 112, 214) 兰紫 */
#define C_PLUM        0x9199   /* RGB(221, 160, 221) 梅子 */

#define C_GOLDEN      0xFEA0   /* RGB(255, 215, 0)   金黄 */
#define C_AMBER       0xFD80   /* RGB(255, 191, 0)   琥珀 */
#define C_TAWNY       0xC365   /* RGB(205, 133, 63)  棕黄 */

/* ================================================================
 *  7. 常见十六进制色（直接给值，写代码里最方便）
 * ================================================================ */
#define C_HEX_000000  0x0000   /* 黑 */
#define C_HEX_FFFFFF  0xFFFF   /* 白 */
#define C_HEX_FF0000  0xF800   /* 红 */
#define C_HEX_00FF00  0x07E0   /* 绿 */
#define C_HEX_0000FF  0x001F   /* 蓝 */
#define C_HEX_FFFF00  0xFFE0   /* 黄 */
#define C_HEX_00FFFF  0x07FF   /* 青 */
#define C_HEX_FF00FF  0xF81F   /* 品红 */
#define C_HEX_808080  0x8410   /* 灰 */
#define C_HEX_FFA500  0xFD20   /* 橙 */
#define C_HEX_800080  0x8010   /* 紫 */
#define C_HEX_FFC0CB  0xFE19   /* 粉 */
#define C_HEX_A52A2A  0xA145   /* 棕 */
#define C_HEX_2F4F4F  0x2967   /* 深灰蓝 */
#define C_HEX_1E90FF  0x1D3F   /* 道奇蓝 */

/* ================================================================
 *  8. 屏保 / 测试用色（纯色，调试用）
 * ================================================================ */
#define C_R            0xF800   /* 纯红，5位满 */
#define C_G            0x07E0   /* 纯绿，6位满 */
#define C_B            0x001F   /* 纯蓝，5位满 */

/* ================================================================
 *  9. 透明色键（用于圆角/精灵图抠图）
 * ================================================================ */
#define C_TRANSPARENT  0xF81F   /* 品红，约定当透明色 */


#endif