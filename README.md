# ST7735S 显示工程 (CH32V307VCT6)

**硬件 SPI2 + DMA** 驱动 1.8 寸 128×160 ST7735S，含局部刷新和 FPS 实时显示。
中文字库模块已备好（当前未启用）。

---

## 1. 接线

| ST7735S 模块 | 功能 | CH32V307 | 说明 |
|---|---|---|---|
| GND | 电源地 | GND | 共地 |
| VCC | 电源正 | **3.3V** | 不要接 5V |
| SCL | 时钟 SCK | **PB13** | SPI2_SCK |
| SDA | 数据 MOSI | **PB15** | SPI2_MOSI |
| RST | 复位 | **PC4** | 低电平复位 |
| DC | 命令/数据 | **PB14** | 0=命令，1=数据 |
| CS | 片选 | **PB12** | 低电平有效 |
| BLK | 背光 | **PC5** | 高电平亮（也可直接接 3.3V） |

> **PB13/PB15 正好是 SPI2 的硬件引脚**，所以本工程默认走**硬件 SPI2**：
> SPI2 挂在 APB1（48MHz）上，默认 2 分频 = **24MHz**；数据还能走
> DMA1_Channel5，刷屏时 CPU 完全不参与。比 IO 模拟 SPI（约 10~12MHz）快一倍以上。
>
> CS/DC/RST/BLK 都是普通推挽输出，任意引脚都行，改 `LCD/config.h` 即可。
> 想换回软件模拟 SPI：把 `LCD_USE_HARD_SPI` 改成 0（此时 SCK/MOSI 可以在
> config.h 里另指定任意两个 IO）。

---

## 2. 工程文件

```
User/
  main.c              例程主程序（局部刷新动画 + FPS 显示）
LCD/
  config.h            引脚定义、SPI 方式/速度、屏参数、显示区偏移
  lcd_port.c/.h       ★ 底层输出层：硬件 SPI2(+DMA) 或软件模拟 SPI，一个开关切换
  lcd.c/.h            ST7735S 驱动（初始化、画点、填充、线/框/圆、位图、精灵、局部刷新）
  fps.c/.h            FPS 统计 + 屏上/串口输出
  digits8x16.c/.h     8x16 数字字母点阵（FPS 显示用，0x20~0x5A）
LCD/WCH原例程_备用/    原来的硬件 SPI+Flash 例程、旧的软件 SPI 实现（不参与编译）
LCD/字库模块_备用/     HZK16 中文字库 + 8x16 英文字库（下一步"取字模画字"用）
tools/               字库/映射表/点阵生成脚本
```

MounRiver Studio 里需要加入编译的源文件：

```
LCD/lcd_port.c      ← ★ 新增, 必须加
LCD/lcd.c
LCD/fps.c
LCD/digits8x16.c
User/main.c  User/ch32v30x_it.c  User/system_ch32v30x.c
```

头文件目录加 `LCD`、`User`。原来的 `LCD/spi_soft.c` 如果还在工程里，**要移出编译**
（已挪到 `LCD/WCH原例程_备用/`）。

> **注意**：原来的 `obj/` 生成目录（里面是与已删除文件对应的 makefile/子目录 mk 和 .o）
> 已经删掉了。在 MounRiver Studio 里点 **Build**（或 Project → Clean 后再 Build）
> 会自动重新生成，不需要任何额外设置。
> 同时确认工程的编译选项是 **Optimization Level = -Os**（本项目默认就是这个），
> 这样 261KB 的中文字库放进 Flash 也不会有问题。

---

## 3. 主程序流程

`main.c` 顶部有两个开关：

```c
#define LCD_OFFSET_TEST_MODE   0   // 0 = 正常例程(当前)
                                   // 1 = 显示偏移自检模式
#define LCD_REFRESH_MODE       1   // 1 = 局部刷新动画(推荐，省传输量、帧率高)
                                   // 0 = 全屏刷新动画(对比用，慢约 40 倍)
```

> 偏移已经在自检里定下来了：`LCD_X_OFFSET = 0`、`LCD_Y_OFFSET = 0`，
> 所以这里保持 0，直接烧就是正常例程。以后换屏再调偏移时改回 1。

### 模式 1：显示偏移自检
屏幕循环切换 4 组偏移值，用来把偏移调准，详见第 5.1 节。

### 模式 0：正常例程
每一步都能单独验证，方便定位问题：

1. `LCD_Init()` → 屏幕亮起并清成黑色
   - 若这一步屏幕就是白的/花的，说明初始化命令或接线有问题
2. `LCD_Clear(LCD_RED / GREEN / BLUE)` → 整屏依次刷红/绿/蓝
   - 三色正常 = 软件 SPI 时序和显存写入都通了
3. `LCD_DrawPoint(10, 10, LCD_RED)` → 在 (10,10) 画红点（并加粗成十字便于观察）
   - **这就是"能画点"的验证**
4. `LCD_DrawLine / LCD_DrawRect / LCD_DrawCircle` → 线、框、圆
5. `LCD_Benchmark()` → 串口打印各项操作实测耗时（对比局部/全屏刷新的差距）
6. 动画：红边框 + 一个青色小球在 40 行高度左右移动
   - `LCD_REFRESH_MODE = 1` 时每帧只擦旧位置 + 画新位置（**局部刷新**）
   - `LCD_REFRESH_MODE = 0` 时每帧全屏清屏再重画（**全屏刷新**，明显慢）

---

## 4. 常用 API

```c
LCD_Init();                                  // 初始化 + 清屏 + 点背光
LCD_BackLight(1);                            // 背光开/关
LCD_Direction(0);                            // 0=竖屏 1=横屏 2=竖屏翻转 3=横屏翻转
LCD_GetWidth(); LCD_GetHeight();             // 当前方向下的宽高

LCD_DrawPoint(x, y, color);                  // 画点
LCD_DrawLine(x0, y0, x1, y1, color);         // 画线
LCD_DrawHLine(x, y, w, color);               // 水平线
LCD_DrawVLine(x, y, h, color);               // 垂直线
LCD_DrawRect(x, y, w, h, color);             // 矩形框
LCD_DrawCircle(xc, yc, r, color);            // 圆（轮廓）
LCD_Fill(x, y, w, h, color);                 // 填充矩形
LCD_Clear(color);                            // 整屏填充

LCD_DrawBitmap(x, y, &bmp, fgColor, bgColor);           // 单色点阵图（字模/图模）
LCD_DrawBitmapScaled(x, y, &bmp, 2, fg, bg);            // 放大 2 倍
LCD_DrawSprite(x, y, &bmp, fgColor, bgColor);           // 精灵：按行整块发送，快
LCD_DrawImage(x, y, w, h, rgb565);                      // 全彩图（RGB565 大端）

LCD_ClearRect(x, y, w, h);                              // 局部擦除（局部刷新用）
LCD_Benchmark();                                        // 实测各操作耗时，串口打印

FPS_Init(1); FPS_Tick(); FPS_Get();                     // FPS 统计（见第 7 节）
LCD_ShowSmallString(x, y, "FPS 60", fg, bg);            // 8x16 小字（0x20~0x5A）
```

颜色是 RGB565，例如：

```c
#define LCD_RED     0xF800
#define LCD_GREEN   0x07E0
#define LCD_BLUE    0x001F
#define LCD_WHITE   0xFFFF
#define LCD_BLACK   0x0000
LCD_RGB888(255, 128, 0)      // 24 位色转 RGB565
```

单色点阵图结构（下一步取字模就是用这个）：

```c
typedef struct {
    uint16_t       width;    // 宽
    uint16_t       height;   // 高
    const uint8_t *data;     // 每行 (width+7)/8 字节，高位在左
} LCD_Bitmap;
```

取模软件设置：**阴码 + 逐行式 + 顺向**，1 表示有笔画。

---

## 5. 显示不正常时怎么调

### 5.1 显示整体偏移 / 有花边 / 滚屏  ← 最常见的问题
1.8 寸 ST7735S 模块的控制器 GRAM 是 **132×162**，可视区只有 **128×160**，
可视区相对 GRAM 左上角有一个偏移，画点前必须把这个偏移加进窗口地址，否则内容就会整体偏移。

偏移量由 `LCD/config.h` 里这两个宏决定：

```c
#define LCD_X_OFFSET      0      // 可视区 X 起点相对 GRAM 的列偏移
#define LCD_Y_OFFSET      0      // 可视区 Y 起点相对 GRAM 的行偏移
```

> **本机（这块 1.8 寸 128×160）实测结论：偏移就是 `(0, 0)`。**
> 自检 4 组里第 4 组 `(0,0)` 完全贴合，已经写进 `config.h`，不用再改。

**调整口诀**（记住方向就不会调反）：

| 现象 | 原因 | 怎么改 |
|------|------|--------|
| 整体**偏左上** | 窗口起点比可视区起点更靠左上 | 把两个值**调小**，先试 `(0,0)` |
| 整体**偏右下** | 窗口起点比可视区起点更靠右下 | 把两个值**调大** |
| 上下滚屏 / 错行 | Y 偏移差 1 | Y 在 `0` 和 `1` 之间试 |
| 左右花边 / 右侧或下侧多出彩条 | 偏移不够，写到了可视区外 | X 试 `2`，Y 试 `0`/`1` |

#### 用自检程序一次调准（推荐）
把 `User/main.c` 顶部的开关设成 1，烧进去：

```c
#define LCD_OFFSET_TEST_MODE   1
```

屏幕会自己循环切换 4 组偏移，每组停 2 秒：

```
(2,1)  ->  (2,0)  ->  (0,1)  ->  (0,0)
```

**判断标准**：红框（外框）正好贴住屏幕四边，而且四个角上的彩色小方块
（红/绿/蓝/白）都完整地落在角上 —— 这一组的偏移就是对的。

屏幕上还会用彩色小方块直接把当前偏移值显示出来（不需要字库）：
- 第一行：白竖线后面有 **1 个红块** = X_OFF 是 1，**2 个红块** = X_OFF 是 2
- 第二行：**绿块个数** = Y_OFF

找到正确值后填进 `config.h`，再把开关改回 0：

```c
#define LCD_OFFSET_TEST_MODE   0
```

常见组合参考：1.8 寸 128×160 多数是 `(2,1)`，也有 `(0,0)` 的批次（**本机这块就是 `(0,0)`**）；
排线在下方的是 `(2,0)`；1.44 寸 128×128 是 `(2,1)`；0.96 寸 80×160 是 `(26,1)`。

#### 自检模式屏幕上的内容对照
| 画面元素 | 含义 |
|----------|------|
| 青色外框 | 逻辑区域 128×160 的边界，**偏移对时正好贴住屏幕四边** |
| 灰色内框 | 往内缩 6 像素，判断有没有内容被推出屏外或绕到对面 |
| 四角彩块（红/绿/蓝/白） | 都被完整裁到角上才算对 |
| 中心十字 + 边缘刻度点 | 刻度每 16 像素一个点，用来数坐标 |
| 第一行白线后的红块个数 | 当前 **X_OFF** 的值（0 个 = 0，1 个 = 1，2 个 = 2） |
| 第二行绿块个数 | 当前 **Y_OFF** 的值 |

4 组对应关系：`(2,1)` → `(2,0)` → `(0,1)` → `(0,0)`（每组 2 秒）。

### 5.2 花屏 / 漏点 / 颜色错位 / 偶发白屏
接线太长或杜邦线太细时，把 SPI 速度调慢，`LCD/config.h`：

```c
#define LCD_SPI_PRESCALER   SPI_BaudRatePrescaler_4   // 24MHz -> 12MHz
//                          SPI_BaudRatePrescaler_2  // 24MHz (默认)
//                          SPI_BaudRatePrescaler_8  //  6MHz (最保守)
```

如果是软件 SPI 模式（`LCD_USE_HARD_SPI = 0`），则改位延时：

```c
#define LCD_SPI_DELAY     4      // 0≈12MHz(默认)  4≈6MHz  16≈2MHz
```

> SPI 时钟不要超过 **24MHz**（ST7735S 的写周期最小 66ns）。
> 如果 SPICLK 提到 96MHz/144MHz 主频，注意分频后的实际频率别超。 

### 5.3 完全白屏（背光亮但无内容）
按顺序排查：
1. `RST`、`DC`、`CS` 是否接对（尤其 DC 接错会全白）
2. 是否 3.3V 供电、是否共地
3. 用示波器/逻辑分析仪看 PC5 是否有波形；没有波形就是程序没跑到
4. 先只跑 `LCD_Init()` + `LCD_Clear(LCD_RED)`，确认基础通路

### 5.4 颜色反了（红蓝互换）
`config.h` 里没有单独开关，改 `lcd.c` 的 `LCD_Direction()`：
把 `MADCTL_BGR` 去掉（改成 `0x00`）就是 RGB 顺序。

### 5.5 关不掉 JTAG 导致 PC0~PC2 不受控
当前接线不再使用 PC0~PC2，这一条只在**把控制脚改回 PC0~PC2** 时才需要：
确认 `lcd_port.c` 的初始化里 GPIO 配置之前有

```c
GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
```

（PC0~PC2 上电默认是 JTAG 复用脚。关掉 JTAG 后 SWD 仍然可用。）

---

## 6. SPI 速度与**局部刷新**（提高帧率的关键）

### 6.1 硬件 SPI2 + DMA 能到多快

| 方式 | 时钟 | 整屏 128×160（40960 字节）耗时 |
|------|------|------------------------------|
| 软件模拟 SPI | ~12MHz | 约 33 ms |
| **硬件 SPI2 2 分频** | **24MHz** | **约 17 ms** |
| 硬件 SPI2 + DMA | 24MHz | 约 17 ms，但 **CPU 完全空闲** |

DMA 的意义不只是快，而是**传输期间 CPU 可以去干别的**（算下一帧、读传感器等）。
本工程的 `LCD_PORT_WriteBlock()` 在数据 ≥16 字节时自动走 DMA。

### 6.2 为什么全屏刷色是浪费

屏上每个像素都要 2 字节，128×160 = **40960 字节**。这部分数据必须一个字节一个字节地推过去，
所以真正决定帧率的是**传输量**，不是 CPU 快不快：

| 操作 | 传输量 | 说明 |
|------|--------|------|
| `LCD_Clear()` 整屏 | **40960 字节** | 每帧都做的话，绝大部分数据是没变化的 |
| 局部擦一块 16×16 | 512 字节 | |
| 画一个 16×16 精灵 | 512 字节 | |
| **局部刷新一帧（擦旧 + 画新）** | **1024 字节** | 比整屏少 **40 倍** |

软件 SPI 下每字节约 0.8~1 µs，所以整屏清屏约 **33 ms**，局部刷新一帧只要 **约 0.8 ms**。
动画的帧率上限就差了约 40 倍。

### 6.3 怎么改成局部刷新

核心就三条：

```c
/* 旧写法: 每帧全屏清屏 —— 40960 字节全白传 */
while (1) {
    LCD_Clear(LCD_BLACK);                  // 慢
    LCD_Fill(x, y, 20, 20, LCD_RED);
    x += 4;
}

/* 新写法: 只处理"变化的两块" —— 上一帧离开的地方 + 这一帧到达的地方 */
uint16_t prev = 0, first = 1;
while (1) {
    if (!first) LCD_ClearRect(prev, y, 20, 20);   // 只擦旧位置(512B)
    LCD_Fill(x, y, 20, 20, LCD_RED);              // 只画新位置(512B)
    prev = x; first = 0;
    x += 4;
}
```

要点：
1. **只刷新变化的小块**，不要 `LCD_Clear()`
2. 用 `LCD_Fill()` / `LCD_DrawSprite()` / `LCD_DrawImage()` **整块发送**，不要逐点 `LCD_DrawPoint()`
   （逐点的话每个点都要重新设置一次窗口，400 个点 ≈ 400 次命令开销）
3. **上一帧的位置一定要擦干净**，否则会拖尾

### 6.4 新增的局部刷新接口

```c
LCD_ClearRect(x, y, w, h);                  // 局部擦除(清成黑色)
LCD_Fill(x, y, w, h, color);                // 局部填充
LCD_DrawSprite(x, y, &bmp, fgColor, bgColor); // 单色点阵精灵, 按行整块发送
LCD_DrawImage(x, y, w, h, rgb565);          // 全彩图, 一次整块发送
LCD_Benchmark();                            // 实测各操作耗时, 串口打印
```

`LCD_DrawSprite()` 特别说明：它按行处理，每行只做 1 次窗口设置，
然后整行数据连续发送，所以画一个 w×h 的图形 = h 次窗口设置 + h 次连续传输，
比 `w*h` 次 `LCD_DrawPoint()` 快几十倍。
限制：`bmp->width` 不能超过 `LCD_FILL_BUF_PIXELS`（`config.h`，默认 128）。

### 6.5 两种动画模式直接对比

`main.c` 顶部：

```c
#define LCD_REFRESH_MODE   1    // 1 = 局部刷新(推荐)  0 = 全屏刷新(对比用)
```

烧进去后串口（115200）会先打印一组实测数据，形如：

```
--- benchmark ---
[BENCH] LCD_Clear()   full 128x160 : 33000 us  (40960 bytes)
[BENCH] LCD_ClearRect  20x20       :   700 us  (800 bytes)
[BENCH] 400 x LCD_DrawPoint()      :  ... us
[BENCH] LCD_Fill        20x20      :   700 us
[BENCH] LCD_DrawImage  20x20 RGB565:   780 us
[BENCH] 局部刷新  20 frames        :  ... us  ->   800 us/frame
[BENCH] 全屏刷新  20 frames        :  ... us  -> 34000 us/frame
```

（具体数值随 `LCD_SPI_DELAY` 和主频变化，但比值基本就是 40 倍左右。）

### 6.6 还能再快的地方

- `LCD_SPI_DELAY` 设成 0（默认）已经是最高速；接线短的话别调大
- 只更新**真正变了的行**：如果新旧位置在同一行且有重叠，重叠部分不用重画
- 动画图形尽量小：传输量只和变化的像素数成正比
- 如果以后要全屏放视频/图片，考虑改用 DMA + 硬件 SPI（`LCD/WCH原例程_备用/` 里就有 DMA 版本可以参考）

---

## 7. FPS 统计与显示

### 7.1 用法（两步）

```c
LCD_Init();              // 先初始化 LCD
FPS_Init(1);             // 1 = 屏幕左上角实时显示   0 = 只统计不显示

while (1)
{
    ... 你的绘制代码 ...
    FPS_Tick();          // ★ 每画完一帧调用一次
}
```

`main.c` 里已经接好了，用开关控制：

```c
#define FPS_ON_SCREEN   1    // 1 = 屏上显示  0 = 只在串口打印
```

### 7.2 屏上显示的内容

屏幕左上角（2,2）用 8×16 小字实时刷新三行：

```
FPS 62.5         当前帧率（500ms 窗口内平均，保留 1 位小数）
T 16000uS        当前帧平均耗时（微秒）
R 61-64          统计窗口内的最低/最高帧率
```

**实现方式**：先在内存里把整个 64×48 面板拼成一块 RGB565 缓冲，
再用 `LCD_DrawImage()` **一次性刷上去**（1 次窗口设置 + 1 次连续传输）。
这样比逐字符画可靠得多，不会和动画互相干扰，也不会画到一半出错。
面板缓冲占 6144 字节 RAM。

### 7.3 FPS 面板的绘制时机与"自愈"（很重要）

面板**不会**在 `FPS_Init()` 里画，而是推迟到**第一次 `FPS_Tick()`**。
原因：很多例程在 `FPS_Init()` 之后还有一次整屏 `LCD_Clear()` 或刷背景，
如果面板在 `FPS_Init()` 里就画好了，会被后面那次清屏刷掉——屏幕上表现为**一闪而过**。

所以初始化顺序必须是：

```c
LCD_Init();

LCD_Clear(LCD_BLACK);      /* 1. 先把背景全部刷完 */
FPS_Init(1);               /* 2. 再打开统计(此刻不画面板) */

Demo_LocalRefresh();       /* 3. 进动画循环; 第一次 FPS_Tick() 时面板才出现 */
```

**自愈机制**：即使数值没变，也每 `FPS_REFRESH_MS`（默认 500ms）强制重画一次面板。
所以哪怕面板那块区域被别的代码刷掉了（整屏清屏、某个图形压过来没调用
`FPS_CheckOverlap`、别的任务乱写显存…），它也一定会自己回来：

```c
#define FPS_REFRESH_MS   500u   /* 设成 0 可关闭自愈 */
```

排查用的两个接口：

```c
FPS_RedrawPanel();                     // 强制重画面板(整屏被清掉之后用)
FPS_CheckOverlap(x, y, w, h);          // 只在这块区域和面板相交时才重画
```

串口会打印诊断，用来确认面板这条路径真的跑到了：

```
switches: OFFSET_TEST=0 REFRESH=1 FPS_ON_SCREEN=1 LCD_TEST=0
[FPS] init: panel 64x48 at (2,2), screen 128x160, visible=1
[FPS] panel #1 drawn to (2,2) 64x48, first line shown
```

如果 **`panel #1 drawn` 出现了但屏幕上没有字**，说明面板确实被发出去了、
只是随后被别的东西刷掉了 —— 自愈机制会在 500ms 内把它补回来。
如果连自愈都补不回来，那一定是**每帧都在清整屏**（例如 `LCD_REFRESH_MODE = 0`），
这种情况要在清屏之后手动调用 `FPS_RedrawPanel()`。

> 面板刷新**不走** `LCD_DrawImage()`，而是逐行 `LCD_SetWindow()` + 连续写字节
> —— 也就是小球精灵 `LCD_DrawSprite()` 在用的同一套底层机制，
> 少一层封装，出问题好定位。

### 7.4 FPS 数字是"实时跟着刷新"的

刷新策略（两条）：

1. **数值一算出来就立刻重画**，不再拖着旧值等下一个周期
2. **只有屏幕上显示的值真的变了才重画**，并且最快 100ms 一次
   —— 所以 FPS 一变马上能看到，又不会每帧都重刷浪费带宽

实现上，`FPS_Redraw()` 会记下这次显示出去的 `FPS/T/min/max`，
下次进 `FPS_Tick()` 时和当前值比较，一样就直接返回。
正常情况每秒重画 2 次左右，每次约占 11ms。

**局部刷新时面板被抹掉怎么办**：如果你的擦除/绘制区域可能压到面板范围，
在每帧刷新完调用一次这个函数，面板会自动补画回来：

```c
LCD_DrawSprite(pos, yBall, &Ball, LCD_CYAN, LCD_BLACK);
FPS_CheckOverlap(pos, yBall, 16, 16);   // 压到了就把面板重画回来
FPS_Tick();
```

不重叠时它只做几次整数比较，开销可以忽略。`main.c` 里已经加好了。

### 7.5 读取接口

```c
FPS_Get();               // 当前 FPS（整数）
FPS_GetX10();            // 当前 FPS * 10（要小数位时用）
FPS_GetFrameUs();        // 当前帧平均耗时（微秒）
FPS_GetMin(); FPS_GetMax();   // 窗口内最低/最高 FPS
FPS_GetTotalFrames();    // 上电以来累计帧数
FPS_Reset();             // 清零重新统计
FPS_Show(0); FPS_Show(1);     // 临时关掉/打开屏上显示
FPS_SetPosition(x, y);   // 换个显示位置
```

例：串口每秒打印一次，不影响屏幕：

```c
FPS_Init(0);
while (1) {
    ...绘制...
    FPS_Tick();
    if (FPS_GetTotalFrames() % 60u == 0u) {
        printf("FPS=%u  frame=%u us\r\n", FPS_Get(), FPS_GetFrameUs());
    }
}
```

### 7.6 计时原理（为什么是准的）

- 计时源用 `SysTick->CNT`：24 位递减计数器，时钟 = `SystemCoreClock/8`
- 换算微秒：`us = 计数值差 / (SystemCoreClock / 8000000)`（144MHz 时除数为 18）
- 144MHz 下计数器约 **932 ms** 溢出一次，统计窗口取 **500 ms**，窗口内不会溢出，读数稳定
- `FPS_Tick()` 里过滤掉小于 20 µs 的间隔，防止误调用把帧率算飞
- 帧率 = 窗口内帧数 × 10000000 / 窗口微秒数，保留 1 位小数

### 7.7 屏上看不到字的排查

`FPS_Init()` 会用串口打印面板信息，先看串口有没有这几行：

```
[FPS] panel 64x48 at (2,2), screen 128x160, visible=1
[FPS] first panel drawn
```

如果没有 → 说明 `LCD/fps.c` 没被加进编译（MounRiver 里检查工程是否包含它）。

如果看到了但屏幕上还是没字，把 `main.c` 里的开关设成 1 烧一次：

```c
#define LCD_TEST_MODE   1
```

自检模式会依次显示：

1. 蓝屏 → 顶部绿条 → 红条
   - **连色块都看不见 = LCD 本身的问题**，不是 FPS 的问题
2. 一行白字 `FPS 1.5 T 1234uS R 0-999`
   - 有色块但没字 → 字模/字库的问题
3. 铺满整张 8×16 字符表（` !"#$%&'()*+,-./0-9:;<=>?@A-Z`）
   - 这里是乱的/空的 → `LCD/digits8x16.c` 没参与编译

### 7.8 实测能到多少

按第 6 节算出来的传输量估算（软件 SPI，`LCD_SPI_DELAY = 0`）：

| 场景 | 每帧传输量 | 帧耗时估算 | 帧率上限 |
|------|-----------|-----------|---------|
| 局部刷新一个 16×16 精灵 | 1024 字节 | ~1 ms + `Delay_Ms(15)` | **由 Delay 决定** |
| 局部刷新，去掉延时 | 1024 字节 | ~1 ms | **约 900 FPS** |
| 全屏清屏 + 画一块 20×20 | 41600 字节 | ~34 ms | **约 29 FPS** |

想看到漂亮的数字就把 `main.c` 里的 `delay_time` 调小或去掉——
那时你会看到 `T` 那一行约 1000 uS、`FPS` 约 900。

> 注意：屏上显示的刷新本身也要传输（3 行 × 十几个字符 × 16 行 × 2 字节），
> 关掉屏上显示（`FPS_Init(0)`）能再多出一点余量。

---

`LCD/字库模块_备用/` 里已经准备好：

| 文件 | 内容 |
|------|------|
| `font.c` / `font.h` | 字库接口：取字模、UTF-8 / GB2312 自动识别 |
| `font_asc.c` | 8x16 ASCII 点阵（0x20~0x7E） |
| `font_hz16.c` | 中文字库（GB2312 全部 8178 个码位，261696 字节，与 HZK16S 同格式） |
| `font_gb2312_map.c` | Unicode ↔ 字库位置映射表（同步生成的，与上面字库严格配对） |

> `tools/hzk16.bin` 和 `font_hz16.c` 是**一次生成、严格配对**的：
> 字库布局 = `(区码-0xA1)*94 + (位码-0xA1)` 个 32 字节字符，和标准 HZK16S 完全一致
> （偏移 1410 起是第一个汉字"啊"）。
> 如果换成网上下的别的 HZK16 文件，**必须同时重新生成 `font_gb2312_map.c`**，
> 或者干脆用本仓库的脚本重新生成整套，否则会显示乱字。
> 字库是从 Windows 系统黑体 SimHei 16px 栅格化来的，字形清晰、自带居中处理，
> 已经用校验程序确认 6763 个汉字全部能正确查表（自检失败数为 0）。

## 8. 下一步：取字模画图（已备好，未启用）

`LCD/字库模块_备用/` 里放着整套中文字库模块，需要时移回 `LCD/` 并加入编译即可：

| 文件 | 内容 |
|------|------|
| `font.c` / `font.h` | 字库接口：取字模、UTF-8 / GB2312 自动识别 |
| `font_asc.c` | 8x16 ASCII 点阵（0x20~0x7E） |
| `font_hz16.c` | 中文字库（GB2312 全部 8178 个码位，261696 字节） |
| `font_gb2312_map.c` | Unicode ↔ 字库位置映射表 |

接上之后就能直接写：

```c
LCD_ShowString(0, 0, "点灯成功", LCD_RED, LCD_BLACK);
LCD_ShowStringScaled(0, 20, "中文", 2, LCD_GREEN, LCD_BLACK);   // 放大 2 倍
LCD_Printf(0, 40, LCD_WHITE, LCD_BLACK, "FPS=%u", FPS_Get());
```

字库也可以自己重新生成（Windows 上跑）：

```powershell
# 重新生成 HZK16 点阵（默认用系统黑体 SimHei 16px 栅格化）
powershell -ExecutionPolicy Bypass -File tools\gen_hzk16.ps1 -Preview

# 重新生成 ASCII 8x16
powershell -ExecutionPolicy Bypass -File tools\gen_asc8x16.ps1

# 二进制转 C 数组
powershell -ExecutionPolicy Bypass -File tools\bin2c.ps1 -InFile tools\hzk16.bin `
           -OutFile LCD\font_hz16.c -Symbol HZK16_FontData

# 重新生成 Unicode <-> GB2312 映射表
powershell -ExecutionPolicy Bypass -File tools\gen_gb2312_uni.ps1

# 重新生成 8x16 数字字母点阵(字形就是脚本里的 ASCII 图案, 改完重跑即可)
python tools\gen_digits8x16.py
```

或用现成的标准 HZK16：见 `tools/download_hzk16.ps1`。

要启用时：
1. 把这 5 个文件移回 `LCD/` 并加入编译
2. 按当时的 `lcd.h` 版本补回 `LCD_ShowString()` 等接口

---

## 9. 本工程已做的编译验证

用 MounRiver 自带工具链（riscv-none-embed-gcc 8.2.0，`-Os -march=rv32imacxw`）实测：

```
FLASH:  59132 B / 288 KB   (20%)     其中约 40KB 是 printf 格式化代码
LINK exit = 0,  编译告警 0
```

四种配置组合都验证过能编译、能链接：

| LCD_USE_HARD_SPI | LCD_USE_DMA | 结果 |
|---|---|---|
| 1（默认，硬件 SPI2） | 1（默认，DMA） | 0 告警，链接通过 |
| 1 | 0（阻塞发送） | 0 告警 |
| 0（软件模拟 SPI） | — | 0 告警 |

链接脚本、启动文件、外设库都沿用原工程（外部链接的 CH32V307EVT SRC），
所以直接在 MounRiver Studio 里 Build 即可，无需改工程配置。