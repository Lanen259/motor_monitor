/**
 * @file    vofa_comms.c
 * @brief   VOFA+ JustFloat 通讯模块实现（单片机侧）
 *
 * 发送：参考原 vofa.c 的 vofa_send_data（逐字节拆 float，小端）
 *       + vofa_sendframetail（追加 0x00 0x00 0x80 0x7F 后发送）。
 * 接收：参考原 vofa_Receive 的 "key=value" 命令解析，改为回调分发。
 */
#include "vofa_comms.h"

#include <stdio.h>  /* snprintf（Vofa_SendCmdText） */
#include <stdlib.h> /* strtod */
#include <string.h> /* memcpy / strchr */

/* ============ 内部状态 ============ */
static uint8_t s_frame[VOFA_MAX_CHANNELS * 4 + 4]; /* 数据 + 帧尾 */
static uint16_t s_frameLen = 0;

static Vofa_TxFn s_txFn = NULL;
static Vofa_CmdFn s_cmdFn = NULL;
static void* s_user = NULL;

#if VOFA_RX_LINE_MODE
static uint8_t s_rxLine[VOFA_RX_LINE_MAX];
static uint16_t s_rxLen = 0;
#endif

/* ============ 初始化 ============ */
void Vofa_Init(Vofa_TxFn tx, Vofa_CmdFn cmdCb, void* user)
{
    s_txFn = tx;
    s_cmdFn = cmdCb;
    s_user = user;
    s_frameLen = 0;
#if VOFA_RX_LINE_MODE
    s_rxLen = 0;
#endif
}

/* ============ 上行：JustFloat 打包 ============ */
void Vofa_StartFrame(void)
{
    s_frameLen = 0;
}

void Vofa_SendData(uint8_t ch, float v)
{
    const uint8_t* p = (const uint8_t*)&v; /* 小端序：byte0 = 最低字节，同原 byte0..byte3 */
    (void)ch;                              /* 通道号仅用于约定顺序，行为与原 vofa_send_data 一致 */

    /* 预留 4 字节给帧尾，确保一帧最多装下 VOFA_MAX_CHANNELS 个 float */
    if (s_frameLen + 4 <= (uint16_t)(sizeof(s_frame) - 4))
    {
        s_frame[s_frameLen++] = p[0];
        s_frame[s_frameLen++] = p[1];
        s_frame[s_frameLen++] = p[2];
        s_frame[s_frameLen++] = p[3];
    }
}

void Vofa_SendFrameTail(void)
{
    if (!s_txFn) return;

#if VOFA_ENABLE_FRAME_TAIL
    s_frame[s_frameLen++] = 0x00;
    s_frame[s_frameLen++] = 0x00;
    s_frame[s_frameLen++] = 0x80;
    s_frame[s_frameLen++] = 0x7F;
#endif

    /* 若回调用 DMA：内部需自备缓冲或等上一帧发完，见 vofa_comms.h 说明 */
    s_txFn(s_frame, s_frameLen, s_user);
    s_frameLen = 0;
}

void Vofa_SendFrame(const float* values, uint8_t n)
{
    uint8_t i;
    if (n > VOFA_MAX_CHANNELS) n = VOFA_MAX_CHANNELS;

    Vofa_StartFrame();
    for (i = 0; i < n; i++) Vofa_SendData(i, values[i]);
    Vofa_SendFrameTail();
}

/* ============ 下行：命令解析 ============ */
static void Vofa_DispatchLine(char* line)
{
    char* eq;
    char* end = NULL;
    double val;

    eq = strchr(line, '=');
    if (!eq) return; /* 非 "key=value" 形式，忽略 */
    *eq = '\0';

    val = strtod(eq + 1, &end);
    if (s_cmdFn) s_cmdFn(line, (float)val, s_user);
}

#if VOFA_RX_LINE_MODE
/* 行帧模式：以 '\n' 或 '\r' 结束一条命令，正确处理半包/粘包 */
void Vofa_FeedRx(const uint8_t* buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        uint8_t c = buf[i];
        if (c == '\n' || c == '\r')
        {
            if (s_rxLen > 0)
            {
                s_rxLine[s_rxLen] = '\0';
                Vofa_DispatchLine((char*)s_rxLine);
                s_rxLen = 0;
            }
        }
        else if (s_rxLen < (uint16_t)(VOFA_RX_LINE_MAX - 1))
        {
            s_rxLine[s_rxLen++] = c;
        }
        else
        {
            s_rxLen = 0; /* 单行超长，丢弃防溢出 */
        }
    }
}
#else
/* 兼容模式：每次调用整段解析为一条命令（等价原 vofa_Receive 的 strstr 行为） */
void Vofa_FeedRx(const uint8_t* buf, uint16_t len)
{
    static char s_cmdBuf[VOFA_RX_LINE_MAX];
    static uint16_t s_cmdLen = 0;
    uint16_t i;

    for (i = 0; i < len && s_cmdLen < (uint16_t)(VOFA_RX_LINE_MAX - 1); i++)
    {
        uint8_t c = buf[i];
        if (c == '\n' || c == '\r') continue;
        s_cmdBuf[s_cmdLen++] = c;
    }
    if (s_cmdLen > 0)
    {
        s_cmdBuf[s_cmdLen] = '\0';
        Vofa_DispatchLine(s_cmdBuf);
        s_cmdLen = 0;
    }
}
#endif

/* ============ 工具 ============ */
void Vofa_SendCmdText(const char* key, float value)
{
    char buf[VOFA_RX_LINE_MAX];
    int n;

    if (!s_txFn) return;
    n = snprintf(buf, sizeof(buf), "%s=%g\r\n", key, value);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
    s_txFn((const uint8_t*)buf, (uint16_t)n, s_user);
}
