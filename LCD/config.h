/*********************************************************************
 * File Name          : config.h
 * Description        : LCD(ST7735S 1.8寸 128x160) 驱动配置文件
 *
 *  接线 (当前实际接线, 8 脚 SPI 模块):
 *    ST7735S 模块 | 功能        | CH32V307 | 说明
 *    ------------------------------------------------------------
 *    GND          | 电源地      | GND      | 共地
 *    VCC          | 电源正      | 3.3V     | 不要接 5V
 *    SCL          | 时钟 SCK    | PB13     | SPI2_SCK
 *    SDA          | 数据 MOSI   | PB15     | SPI2_MOSI
 *    RST          | 复位        | PC4      | 低电平复位
 *    DC           | 命令/数据   | PB14     | 0=命令 1=数据
 *    CS           | 片选        | PB12     | 低电平有效
 *    BLK          | 背光        | PC5      | 高电平亮(也可直接接 3.3V)
 *
 *  注意: PB13/PB15 正好是 SPI2 的硬件引脚, PB12/PB14 可以当普通 IO 用也可以
 *        当 SPI2 的 NSS 用。所以本工程默认走**硬件 SPI2**(最快),
 *        也保留了软件模拟 SPI 的代码, 改 LCD_USE_HARD_SPI 就能切换。
 *******************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

/*===========================================================================
 *  0. SPI 方式选择
 *      1 = 硬件 SPI2 (推荐, 快; 需要 SCL=PB13 / SDA=PB15)
 *      0 = 软件模拟 SPI (任何引脚都行, 慢一些)
 *=========================================================================*/
#define LCD_USE_HARD_SPI     1

/* 1 = 用 DMA 搬运像素数据(刷屏/局部刷新最快)
 * 只有硬件 SPI 模式下有效 */
#define LCD_USE_DMA          1

/*===========================================================================
 *  1. 引脚定义
 *  CS / DC / RST / BLK 都是普通推挽输出; SCK / MOSI 在硬件 SPI 模式下
 *  由 SPI2 外设接管(复用推挽), 软件模式下才由程序翻转电平。
 *=========================================================================*/
#define LCD_CS_PORT       GPIOB
#define LCD_CS_PIN        GPIO_Pin_12    /* PB12 片选, 低有效      */
#define LCD_CS_RCC        RCC_APB2Periph_GPIOB

#define LCD_DC_PORT       GPIOB
#define LCD_DC_PIN        GPIO_Pin_14    /* PB14 0=命令 1=数据     */
#define LCD_DC_RCC        RCC_APB2Periph_GPIOB

#define LCD_RST_PORT      GPIOC
#define LCD_RST_PIN       GPIO_Pin_4     /* PC4  复位, 低电平复位  */
#define LCD_RST_RCC       RCC_APB2Periph_GPIOC

#define LCD_BL_PORT       GPIOC
#define LCD_BL_PIN        GPIO_Pin_5     /* PC5  背光, 高电平点亮  */
#define LCD_BL_RCC        RCC_APB2Periph_GPIOC

/* 硬件 SPI: PB13 = SCK, PB15 = MOSI (SPI2 的固定复用脚) */
#define LCD_SPI_PORT_NUM  2
#define LCD_SCK_PIN       GPIO_Pin_13
#define LCD_MOSI_PIN      GPIO_Pin_15
#define LCD_SPI_GPIO_RCC  RCC_APB2Periph_GPIOB

/* 背光极性: 1 = 高电平点亮(默认), 0 = 低电平点亮 */
#define LCD_BL_ACTIVE_HIGH   1

/*===========================================================================
 *  2. 屏参数
 *  1.8 寸 ST7735S 模块: 可视区 128x160, 控制器 GRAM 132x162。
 *  可视区相对 GRAM 左上角的偏移就是下面这两个值 —— 显示整体偏移全靠它们。
 *
 *  偏移怎么定:
 *      可视区水平居中时  可视X起点 = (132 - 可视宽) / 2 = (132-128)/2 = 2
 *      所以 LCD_X_OFFSET 通常是 2;
 *      LCD_Y_OFFSET 取决于排线方向, 128x160 面板常见是 1。
 *
 *  ★ 显示"整体偏左上" -> 窗口要往右下推 -> 把这两个值调小
 *    显示"整体偏右下" -> 把这两个值调大
 *    显示"滚屏/上下错行/左右花边" -> 多半是 Y 偏移差 1, 在 0 和 1 之间试
 *
 *  【注意】本机实测结论是针对上一块屏(PC0 接线那套)做的: (0,0) 完全贴合。
 *         现在换了新接线, 如果显示有偏移, 重新确认一次:
 *         把 main.c 里的 LCD_OFFSET_TEST_MODE 设成 1 烧一次, 屏幕会循环
 *         切 4 组偏移, 哪种"红框正好贴住四边"就用哪组。
 *
 *  常见组合:
 *      1.8" 128x160  上一块屏实测     (0,0)
 *      1.8" 128x160  红板/黑板/蓝板    (2,1)
 *      1.8" 128x160  排线在下          (2,0)
 *      1.44" 128x128                   (2,1)
 *      0.96" 80x160                    (26,1)
 *=========================================================================*/
#define LCD_X_OFFSET      0
#define LCD_Y_OFFSET      0

/* 物理分辨率(可视区) */
#define LCD_PHY_W         128
#define LCD_PHY_H         160

/* 上电默认显示方向: 0=竖屏(128x160) 1=横屏(160x128) 2=竖屏翻转 3=横屏翻转 */
#define LCD_DEFAULT_DIR   0

/* 颜色格式: RGB565 = 16bit/像素 */
#define LCD_COLOR_BITS    16

/*===========================================================================
 *  3. SPI 速度
 *=========================================================================*/
#if (LCD_USE_HARD_SPI)
/* 硬件 SPI 分频。SPI2 挂在 APB1(默认 48MHz)上:
 *      SPI_BaudRatePrescaler_2  -> 24MHz  <-- 默认, ST7735S 上限附近
 *      SPI_BaudRatePrescaler_4  -> 12MHz  <-- 接线较长/不稳时用这个
 *      SPI_BaudRatePrescaler_8  ->  6MHz  <-- 最保守
 *   出现花屏/漏点就把分频改大。用高主频时也要注意别超过 24MHz。 */
#define LCD_SPI_PRESCALER   SPI_BaudRatePrescaler_2
#else
/* 软件模拟 SPI 每一位的空指令延时:
 *      0  -> 约 10~12MHz (默认)
 *      4  -> 约  6MHz
 *      16 -> 约  2MHz
 *   接线长/不稳时把值调大。 */
#define LCD_SPI_DELAY     0
#endif

/* 大块数据搬运用的缓冲区像素数(刷屏/局部填充用)
 * 占用的 RAM = LCD_BULK_PIXELS * 2 字节 */
#define LCD_BULK_PIXELS   128u

#endif /* __CONFIG_H__ */
