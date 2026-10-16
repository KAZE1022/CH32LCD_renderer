/*********************************************************************
 * File Name          : fps.c
 * Description        : FPS 统计 + 屏幕显示 / 串口打印
 *
 *   屏上显示的实现方式(关键):
 *      先在内存里把整个 FPS 面板拼成一块 RGB565 缓冲区(FPS_Panel),
 *      再逐行 LCD_SetWindow() + 连续写字节刷到屏幕左上角。
 *      这里刻意不用 LCD_DrawImage(), 而是用小球精灵(LCD_DrawSprite)
 *      同一套底层机制, 少一层封装, 出问题好定位。
 *
 *   面板"保鲜"机制:
 *      FPS_Init() 不画面板(避免被后面的清屏刷掉, 表现为"一闪而过")。
 *      第一次 FPS_Tick() 才画; 之后数值一变立刻重画;
 *      另外超过 FPS_REFRESH_MS 没画过就强制重画一次(自愈),
 *      所以就算面板那块被别的代码刷掉, 也一定会自己回来。
 *
 *   计时源: SysTick->CNT
 *      24 位递减计数, 每 (SystemCoreClock/8) 个计数值 1 秒
 *      微秒换算:  us = 计数值差 / (SystemCoreClock / 8000000)
 *      144MHz 时除数 = 18, 计数器约 932ms 溢出一次
 *      统计窗口取 500ms, 窗口内不会溢出, 读数稳定
 *******************************************************************************/

#include "fps.h"
#include "lcd.h"
#include "digits8x16.h"

/* 统计窗口长度(毫秒): 决定"多久结算一次数值" */
#define FPS_WIN_MS          500u

/* 面板最小重画间隔(毫秒): 数值变了也就最多这么频繁地刷屏 */
#define FPS_DRAW_MIN_MS     100u

/* 面板最长"保鲜期"(毫秒): 超过这么久没画过就强制重画一次。
 * 作用是"自愈"——万一有别的代码把面板那块区域刷掉了(比如整屏清屏、
 * 或者某个图形压过来没调用 FPS_CheckOverlap), 也一定会被补回来。
 * 设成 0 表示关闭这个自愈功能。 */
#define FPS_REFRESH_MS      500u

/* 调用 FPS_Tick() 的最小间隔(微秒), 小于这个的抖动不计入 */
#define FPS_TICK_MIN_US     20u

/* 面板尺寸: 8 个 8x16 字符宽 = 64, 3 行 * 16 = 48 */
#define FPS_PANEL_W         64u
#define FPS_PANEL_H         48u

static uint16_t FPS_X        = 2;        /* 面板左上角 */
static uint16_t FPS_Y        = 2;
static uint8_t  FPS_Visible  = 1;
static uint8_t  FPS_Inited   = 0;

static uint32_t FPS_LastTick = 0;
static uint32_t FPS_WinStart = 0;
static uint32_t FPS_WinUs    = FPS_WIN_MS * 1000u;
static uint16_t FPS_Frames   = 0;

static uint16_t FPS_x10      = 0;        /* 当前 FPS * 10 */
static uint16_t FPS_Min10    = 0xFFFFu;
static uint16_t FPS_Max10    = 0;
static uint16_t FPS_FrameUs  = 0;
static uint32_t FPS_Total    = 0;

static uint16_t FPS_ShownX10     = 0xFFFFu;   /* 屏幕上正在显示的值(用来判断要不要重画) */
static uint16_t FPS_ShownFrameUs = 0xFFFFu;
static uint16_t FPS_ShownMin10   = 0xFFFFu;
static uint16_t FPS_ShownMax10   = 0xFFFFu;
static uint8_t  FPS_ShownValid   = 0;
static uint8_t  FPS_ForceDraw    = 0;         /* 1 = 下一次 Tick 必须重画面板 */
static uint32_t FPS_LastDrawTick = 0;         /* 上次重画时的 SysTick 计数 */

/* 面板缓冲区: 64*48*2 = 6144 字节 */
static uint16_t FPS_Panel[FPS_PANEL_W * FPS_PANEL_H];

/*===========================================================================
 *  内部: 微秒计时
 *=========================================================================*/
static uint32_t FPS_UsPerTick(void)
{
    uint32_t d = SystemCoreClock / 8000000u;
    return d ? d : 1u;
}

/* 把 SysTick 计数差换算成毫秒(用 64 位中间量, 防止大主频下溢出)
 * 注意 SysTick 是递减计数, 计数差 = 过去 - 现在 */
static uint32_t FPS_DeltaMs(uint32_t from, uint32_t now)
{
    uint32_t diff;

    if (now <= from)
    {
        diff = from - now;
    }
    else
    {
        diff = from + (0x01000000u - now);      /* 处理回绕 */
    }
    return (uint32_t)(((uint64_t)diff * 1000u) / ((uint64_t)FPS_UsPerTick() * 1000u));
}

static uint32_t FPS_ElapsedUs(void)
{
    uint32_t now  = SysTick->CNT;
    uint32_t diff;

    if (now <= FPS_LastTick)
    {
        diff = FPS_LastTick - now;
    }
    else
    {
        diff = FPS_LastTick + (0x01000000u - now);   /* 处理回绕 */
    }
    FPS_LastTick = now;

    return diff / FPS_UsPerTick();
}

/*===========================================================================
 *  内部: 字符串工具(不用 sprintf, 省 Flash)
 *=========================================================================*/
static void FPS_U32ToStr(uint32_t v, char *buf)
{
    char tmp[12];
    int  n = 0;
    int  i = 0;

    if (v == 0)
    {
        buf[0] = '0';
        buf[1] = 0;
        return;
    }
    while (v > 0 && n < 11)
    {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0)
    {
        buf[i++] = tmp[--n];
    }
    buf[i] = 0;
}

static void FPS_Append(char *dst, const char *src)
{
    while (*dst)
    {
        dst++;
    }
    while (*src)
    {
        *dst++ = *src++;
    }
    *dst = 0;
}

/*===========================================================================
 *  内部: 往面板缓冲里直接画一个字符(8x16 点阵)
 *  这里刻意不调用 LCD_DrawSpriteEx, 直接操作缓冲, 每一步都可见可控,
 *  避免中间环节静默失败导致"面板是空的"
 *=========================================================================*/
static void FPS_PanelDrawChar(uint16_t px, uint16_t py, char ch,
                              uint16_t color, uint16_t bg)
{
    const uint8_t *g = DIGITS8X16_PTR((uint8_t)ch);
    uint16_t row, col;

    for (row = 0; row < DIGITS8X16_HEIGHT; row++)
    {
        uint16_t iy = (uint16_t)(py + row);
        uint8_t  bits;

        if (iy >= FPS_PANEL_H)
        {
            break;
        }
        bits = g[row];

        for (col = 0; col < DIGITS8X16_WIDTH; col++)
        {
            uint16_t ix = (uint16_t)(px + col);

            if (ix >= FPS_PANEL_W)
            {
                break;
            }
            FPS_Panel[(uint32_t)iy * FPS_PANEL_W + ix] =
                (bits & (0x80u >> col)) ? color : bg;
        }
    }
}

static void FPS_PanelDrawStr(uint16_t x, uint16_t y, const char *str,
                             uint16_t color, uint16_t bg)
{
    while (*str)
    {
        FPS_PanelDrawChar(x, y, *str, color, bg);
        x = (uint16_t)(x + DIGITS8X16_WIDTH);
        str++;
    }
}

/*********************************************************************
 * @fn      FPS_PanelFlush
 * @brief   把面板缓冲刷到屏幕上
 *
 *   这里不用 LCD_DrawImage(), 而是逐行 LCD_SetWindow() + 直接连续写字节,
 *   也就是小球精灵(LCD_DrawSprite) 已经在用的同一套底层机制。
 *   少一层封装, 每一步都可见, 出问题也好定位。
 *********************************************************************/
static void FPS_PanelFlush(void)
{
    uint16_t row, col;

    for (row = 0; row < FPS_PANEL_H; row++)
    {
        uint16_t py = (uint16_t)(FPS_Y + row);

        if (py >= LCD_GetHeight())
        {
            break;
        }

        LCD_SetWindow(FPS_X, py, (uint16_t)(FPS_X + FPS_PANEL_W - 1u), py);
        LCD_PORT_BeginData();
        for (col = 0; col < FPS_PANEL_W; col++)
        {
            uint16_t c = FPS_Panel[(uint32_t)row * FPS_PANEL_W + col];
            LCD_PORT_WriteByte((uint8_t)(c >> 8));      /* RGB565 高字节在前 */
            LCD_PORT_WriteByte((uint8_t)c);
        }
        LCD_PORT_End();
    }
}

/*********************************************************************
 * @fn      FPS_Redraw
 * @brief   重新拼面板并刷到屏幕
 *********************************************************************/
static void FPS_Redraw(void)
{
    char     line[24];
    char     tmp[12];
    uint16_t v;
    uint32_t i;

    if (!FPS_Visible)
    {
        return;
    }

    /* 整块面板先铺成黑底 */
    for (i = 0; i < (uint32_t)FPS_PANEL_W * FPS_PANEL_H; i++)
    {
        FPS_Panel[i] = LCD_BLACK;
    }

    /* --- 第 1 行: FPS xx.x --- */
    line[0] = 0;
    FPS_Append(line, "FPS ");
    FPS_U32ToStr((uint32_t)(FPS_x10 / 10u), tmp);
    FPS_Append(line, tmp);
    if (FPS_x10 < 10000u)
    {
        FPS_Append(line, ".");
        tmp[0] = (char)('0' + (FPS_x10 % 10u));
        tmp[1] = 0;
        FPS_Append(line, tmp);
    }
    FPS_PanelDrawStr(0, 0, line, LCD_GREEN, LCD_BLACK);

    /* --- 第 2 行: T xxxxuS --- */
    line[0] = 0;
    FPS_Append(line, "T ");
    FPS_U32ToStr((uint32_t)FPS_FrameUs, tmp);
    FPS_Append(line, tmp);
    FPS_Append(line, "uS");
    FPS_PanelDrawStr(0, 16, line, LCD_CYAN, LCD_BLACK);

    /* --- 第 3 行: R min-max --- */
    line[0] = 0;
    FPS_Append(line, "R ");
    v = (FPS_Min10 == 0xFFFFu) ? 0u : (uint16_t)(FPS_Min10 / 10u);
    FPS_U32ToStr((uint32_t)v, tmp);
    FPS_Append(line, tmp);
    FPS_Append(line, "-");
    FPS_U32ToStr((uint32_t)(FPS_Max10 / 10u), tmp);
    FPS_Append(line, tmp);
    FPS_PanelDrawStr(0, 32, line, LCD_YELLOW, LCD_BLACK);

    /* 刷上去: 逐行 LCD_SetWindow + 连续写, 和小球精灵同一套底层机制 */
    FPS_PanelFlush();

    /* 第一次画的时候在串口留个记录, 方便确认这条路径真的跑到了 */
    {
        static uint8_t logged = 0;
        if (!logged)
        {
            logged = 1;
            printf("[FPS] panel #1 drawn to (%u,%u) %ux%u, first line shown\r\n",
                   (unsigned int)FPS_X, (unsigned int)FPS_Y,
                   (unsigned int)FPS_PANEL_W, (unsigned int)FPS_PANEL_H);
        }
    }

    /* 记下这次显示出去的值, 下次只有变了才需要重画 */
    FPS_ShownX10     = FPS_x10;
    FPS_ShownFrameUs = FPS_FrameUs;
    FPS_ShownMin10   = FPS_Min10;
    FPS_ShownMax10   = FPS_Max10;
    FPS_ShownValid   = 1;
    FPS_LastDrawTick = SysTick->CNT;
}

/*********************************************************************
 * @fn      FPS_RedrawIfChanged
 * @brief   只有"屏幕上显示的值"和"当前值"不一样时才重画
 *          这样 FPS 一变就能立刻看到, 但不会每帧都重画浪费带宽
 *********************************************************************/
static void FPS_RedrawIfChanged(void)
{
    uint32_t now;
    uint32_t elapsedMs;

    if (!FPS_Visible)
    {
        return;
    }
    if (FPS_ForceDraw || !FPS_ShownValid)
    {
        FPS_ForceDraw = 0;
        FPS_Redraw();
        return;
    }

    /* 自愈: 不管值变没变, 超过 FPS_REFRESH_MS 没画过就强制重画一次。
       这样即使面板被别的代码刷掉, 也一定会自己回来。 */
    now = SysTick->CNT;
    elapsedMs = FPS_DeltaMs(FPS_LastDrawTick, now);

    if (elapsedMs >= FPS_REFRESH_MS)
    {
        FPS_Redraw();
        return;
    }

    /* 值没变就不画 */
    if (FPS_ShownX10 == FPS_x10 && FPS_ShownFrameUs == FPS_FrameUs &&
        FPS_ShownMin10 == FPS_Min10 && FPS_ShownMax10 == FPS_Max10)
    {
        return;
    }

    /* 即使变了, 最快也要隔 FPS_DRAW_MIN_MS 才重画一次, 免得刷太频繁 */
    if (elapsedMs >= FPS_DRAW_MIN_MS)
    {
        FPS_Redraw();
    }
}

/*********************************************************************
 * @fn      FPS_CheckOverlap
 * @brief   检查某个矩形是否压到了 FPS 面板区域; 压到了就把面板重画回来
 *
 *   局部刷新时如果在面板范围内擦除/覆盖, 面板上的数字就会被抹掉。
 *   每帧刷新完调一次这个函数, 面板就会自动"浮在最上层", 数字一直可见。
 *   不重叠时这个函数只做几次整数比较, 开销可以忽略。
 *********************************************************************/
void FPS_CheckOverlap(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (!FPS_Visible)
    {
        return;
    }
    if (x >= (uint16_t)(FPS_X + FPS_PANEL_W) || (uint16_t)(x + w) <= FPS_X)
    {
        return;
    }
    if (y >= (uint16_t)(FPS_Y + FPS_PANEL_H) || (uint16_t)(y + h) <= FPS_Y)
    {
        return;
    }

    LCD_DrawImage(FPS_X, FPS_Y, FPS_PANEL_W, FPS_PANEL_H, (const uint8_t *)FPS_Panel);
}

/*********************************************************************
 * @fn      FPS_RedrawPanel
 * @brief   强制把面板重画一次(不管数值有没有变)
 *          用在"整屏被清掉"之后, 例如全屏刷新例程里每帧 LCD_Clear() 之后
 *********************************************************************/
void FPS_RedrawPanel(void)
{
    if (FPS_Visible)
    {
        FPS_Redraw();
    }
}

/*===========================================================================
 *  对外接口
 *=========================================================================*/
void FPS_Init(uint8_t visible)
{
    FPS_Visible    = visible ? 1u : 0u;
    FPS_Inited     = 1;
    FPS_ForceDraw  = 1;         /* ★ 不在这里画! 等第一次 FPS_Tick() 再画 */

    FPS_Reset();

    /* 这里故意不调用 FPS_Redraw():
       很多例程在 FPS_Init() 之后还会 LCD_Clear() 整屏或者刷一次背景,
       如果在这里画就会被后面那次清屏刷掉, 屏幕上表现为"一闪而过"。
       所以把首次绘制推迟到第一次 FPS_Tick(), 那时画面已经稳定了。 */
    printf("[FPS] init: panel %ux%u at (%u,%u), screen %ux%u, visible=%u\r\n",
           (unsigned int)FPS_PANEL_W, (unsigned int)FPS_PANEL_H,
           (unsigned int)FPS_X, (unsigned int)FPS_Y,
           (unsigned int)LCD_GetWidth(), (unsigned int)LCD_GetHeight(),
           (unsigned int)FPS_Visible);
}

void FPS_SetPosition(uint16_t x, uint16_t y)
{
    FPS_X = x;
    FPS_Y = y;
    if (FPS_Visible)
    {
        FPS_Redraw();
    }
}

void FPS_Show(uint8_t visible)
{
    FPS_Visible = visible ? 1u : 0u;
    if (FPS_Visible)
    {
        FPS_Redraw();
    }
    else
    {
        /* 关掉显示时把那块擦干净 */
        LCD_ClearRect(FPS_X, FPS_Y, FPS_PANEL_W, FPS_PANEL_H);
    }
}

void FPS_Reset(void)
{
    FPS_Frames   = 0;
    FPS_Total    = 0;
    FPS_x10      = 0;
    FPS_Min10    = 0xFFFFu;
    FPS_Max10    = 0;
    FPS_FrameUs  = 0;
    FPS_WinUs    = FPS_WIN_MS * 1000u;
    FPS_LastTick = SysTick->CNT;
    FPS_WinStart = FPS_LastTick;
}

/*********************************************************************
 * @fn      FPS_Tick
 * @brief   每完成一帧调用一次
 *********************************************************************/
void FPS_Tick(void)
{
    uint32_t elapsedUs;
    uint32_t now;

    if (!FPS_Inited)
    {
        FPS_Init(FPS_Visible);
    }

    elapsedUs = FPS_ElapsedUs();

    if (elapsedUs < FPS_TICK_MIN_US)
    {
        return;
    }

    FPS_Frames++;
    FPS_Total++;

    now = SysTick->CNT;

    {
        uint32_t winElapsed;

        if (now <= FPS_WinStart)
        {
            winElapsed = FPS_WinStart - now;
        }
        else
        {
            winElapsed = FPS_WinStart + (0x01000000u - now);
        }
        winElapsed /= FPS_UsPerTick();          /* 微秒 */

        if (winElapsed >= FPS_WinUs)
        {
            uint32_t fps10 = (uint32_t)FPS_Frames * 10000000u / winElapsed;

            if (fps10 > 65535u)
            {
                fps10 = 65535u;
            }
            FPS_x10 = (uint16_t)fps10;

            if (FPS_x10 < FPS_Min10)
            {
                FPS_Min10 = FPS_x10;
            }
            if (FPS_x10 > FPS_Max10)
            {
                FPS_Max10 = FPS_x10;
            }

            FPS_FrameUs = (uint16_t)(winElapsed / (FPS_Frames ? FPS_Frames : 1u));

            FPS_WinStart = now;
            FPS_Frames   = 0;
        }
    }

    /* ★ 每次 Tick 都检查一遍: 首次(值还是初始的)以及数值一变就立刻刷新,
       所以画面稳定之后面板马上就会出现, 而且一直跟着最新值走 */
    FPS_RedrawIfChanged();
}

uint16_t FPS_Get(void)
{
    return (uint16_t)(FPS_x10 / 10u);
}

uint16_t FPS_GetX10(void)
{
    return FPS_x10;
}

uint16_t FPS_GetFrameUs(void)
{
    return FPS_FrameUs;
}

uint16_t FPS_GetMin(void)
{
    return (FPS_Min10 == 0xFFFFu) ? 0u : (uint16_t)(FPS_Min10 / 10u);
}

uint16_t FPS_GetMax(void)
{
    return (uint16_t)(FPS_Max10 / 10u);
}

uint32_t FPS_GetTotalFrames(void)
{
    return FPS_Total;
}

/*********************************************************************
 * @fn      FPS_Test
 * @brief   自检: 在屏幕上画一块纯色区域 + 一行 8x16 字符 "FPS 1.5" + 字符表
 *          用来确认"屏上小字显示"这条链路是通的。
 *          如果连这块纯色都看不见, 那就是 LCD 本身的问题, 不是 FPS 的问题。
 *********************************************************************/
void FPS_Test(void)
{
    static const char *sample = "FPS 1.5 T 1234uS R 0-999";
    uint16_t i;

    printf("=== FPS_Test start ===\r\n");

    /* 1) 满屏纯色: 先确认能画 */
    printf("step1: LCD_Clear(BLUE)\r\n");
    LCD_Clear(LCD_BLUE);
    Delay_Ms(700);

    printf("step2: green bar\r\n");
    LCD_Clear(LCD_BLACK);
    LCD_Fill(0, 0, LCD_GetWidth(), 20, LCD_GREEN);
    Delay_Ms(700);

    printf("step3: red bar\r\n");
    LCD_Fill(0, 22, LCD_GetWidth(), 20, LCD_RED);
    Delay_Ms(700);

    /* 2) 用 8x16 点阵写一行字(白字黑底) */
    printf("step4: LCD_ShowSmallString\r\n");
    LCD_ShowSmallString(2, 48, sample, LCD_WHITE, LCD_BLACK);
    Delay_Ms(1500);

    /* 3) ★ 关键: 用真实面板路径显示一组固定值
          FPS_RedrawPanel() 就是正常显示数字时用的那条路径。
          如果这一步屏幕上没有三行字(64x48 那块), 说明 LCD_DrawImage 有问题;
          如果有, 说明画图链路没问题, 问题在动画循环那边。 */
    printf("step5: force FPS panel (real path)\r\n");
    LCD_Clear(LCD_BLACK);
    FPS_x10     = 625;          /* 62.5 */
    FPS_FrameUs = 16000;
    FPS_Min10   = 610;          /* 61.0 */
    FPS_Max10   = 640;          /* 64.0 */
    FPS_Visible = 1;
    FPS_RedrawPanel();
    printf("step5 done: panel should be at (%u,%u) size %ux%u\r\n",
           (unsigned int)FPS_X, (unsigned int)FPS_Y,
           (unsigned int)FPS_PANEL_W, (unsigned int)FPS_PANEL_H);
    Delay_Ms(2500);

    /* 4) 把整个 8x16 字符表铺在屏幕上(9 列), 检查字模
          如果这里是乱的/空的, 说明 LCD\digits8x16.c 没参与编译 */
    printf("step6: full 8x16 charset\r\n");
    LCD_Clear(LCD_BLACK);
    for (i = 0; i < DIGITS8X16_COUNT; i++)
    {
        LCD_Bitmap bmp;
        char s[2];
        bmp.width  = DIGITS8X16_WIDTH;
        bmp.height = DIGITS8X16_HEIGHT;
        s[0] = (char)(DIGITS8X16_FIRST + i);
        s[1] = 0;
        bmp.data = DIGITS8X16_PTR((uint8_t)s[0]);
        LCD_DrawSprite((uint16_t)((i % 9u) * 9u + 28u),
                       (uint16_t)((i / 9u) * 20u + 4u),
                       &bmp, LCD_WHITE, LCD_BLACK);
    }
    printf("=== FPS_Test done ===\r\n");
    Delay_Ms(3000);
}

/*===========================================================================
 *  8x16 小字(直接画到屏幕上, 给别的调试信息用)
 *=========================================================================*/
uint16_t LCD_ShowSmallString(uint16_t x, uint16_t y, const char *str,
                             uint16_t color, uint16_t bgColor)
{
    uint16_t cx = x;
    LCD_Bitmap bmp;

    if (str == NULL)
    {
        return 0;
    }

    bmp.width  = DIGITS8X16_WIDTH;
    bmp.height = DIGITS8X16_HEIGHT;

    while (*str)
    {
        bmp.data = DIGITS8X16_PTR((uint8_t)(*str));
        LCD_DrawSprite(cx, y, &bmp, color, bgColor);
        cx = (uint16_t)(cx + DIGITS8X16_WIDTH);
        str++;
    }

    return (uint16_t)(cx - x);
}
