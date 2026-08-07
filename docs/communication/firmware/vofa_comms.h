/**
 * @file    vofa_comms.h
 * @brief   VOFA+ JustFloat 通讯模块（单片机侧）— 通用可移植
 *
 * 协议（与原 vofa.c 完全一致，参考其 vofa_transmit / vofa_send_data /
 *        vofa_sendframetail / vofa_Receive 的底层实现）：
 *
 *   上行 JustFloat：
 *       每个通道 4 字节 float（小端序），帧尾 0x00 0x00 0x80 0x7F。
 *   下行命令：
 *       文本 "key=value"，以 '\n' 或 '\r' 结尾，例如 "speed=1200\r\n"。
 *
 * 与原 vofa.c 函数对应关系：
 *   vofa_transmit()      <->  Vofa_Init() 中注册的发送回调（你只改这一处对接 HAL）
 *   vofa_send_data()     <->  Vofa_SendData()
 *   vofa_sendframetail() <->  Vofa_SendFrameTail()
 *   vofa_start()         <->  Vofa_StartFrame() + Vofa_SendData()... + Vofa_SendFrameTail()
 *   vofa_dma_recv()      <->  Vofa_FeedRx()
 *   vofa_Receive()       <->  Vofa_FeedRx() 内部（命令回调分发）
 *
 * 函数均带大写 Vofa_ 前缀，可与工程内已有的 vofa.c 并存不冲突。
 */
#ifndef VOFA_COMMS_H
#define VOFA_COMMS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置区（编译前可覆盖）
 * ============================================================ */
#ifndef VOFA_MAX_CHANNELS
#define VOFA_MAX_CHANNELS 32 /* 一帧最多 float 通道数 */
#endif

#ifndef VOFA_ENABLE_FRAME_TAIL
#define VOFA_ENABLE_FRAME_TAIL 1
/* =1 追加 0x00 0x00 0x80 0x7F（VOFA+ JustFloat 标准帧尾，默认开）
 * =0 不追加帧尾。
 * 注意：本仓库 Qt 上位机 MotorStudio 的 VofaParser 按固定通道数切帧、
 *       不识别帧尾。若直连它且保留帧尾，需把上位机通道数设为
 *       "实际通道数+1"（帧尾会被解析成一个 +inf 通道）；或者把本宏置 0。 */
#endif

#ifndef VOFA_RX_LINE_MODE
#define VOFA_RX_LINE_MODE 1
/* =1 行帧模式：命令以 '\n' 或 '\r' 结束（推荐，抗半包/粘包）
 * =0 兼容模式：每次 Vofa_FeedRx() 调用把整段解析为一条命令
 *    （等价原 vofa_Receive 的 strstr 行为，适合一包一条命令的 DMA 空闲收法） */
#endif

#ifndef VOFA_RX_LINE_MAX
#define VOFA_RX_LINE_MAX 96 /* 接收命令行缓冲上限（含结尾 '\0'） */
#endif

/* ============================================================
 * 回调类型
 * ============================================================ */
/* 底层发送回调：把 len 字节发出去（USART / USB / DMA 均可，由你实现）。
 *
 * 例（对接原 vofa_transmit）：
 *   static void my_tx(const uint8_t* d, uint16_t n, void* u) {
 *       (void)u;
 *       memcpy(send_buf, d, n);                  // 自备 DMA 缓冲，防覆盖
 *       HAL_UART_Transmit_DMA(&huart6, send_buf, n);
 *   }
 *
 * 重要：若使用 DMA，回调内务必 memcpy 到独立缓冲后再触发 DMA，
 *       或等待上一帧发送完成——否则 s_frame 可能在 DMA 未结束时被下一帧覆盖。
 */
typedef void (*Vofa_TxFn)(const uint8_t* data, uint16_t len, void* user);

/* 命令回调：解析出 "key=value" 时调用。key 指向模块内部缓冲，仅本次调用内有效。 */
typedef void (*Vofa_CmdFn)(const char* key, float value, void* user);

/* ============================================================
 * API
 * ============================================================ */
/* 初始化。tx: 发送回调；cmdCb: 命令回调；user: 透传指针（可为 NULL）。 */
void Vofa_Init(Vofa_TxFn tx, Vofa_CmdFn cmdCb, void* user);

/* ---- 上行：JustFloat 打包 ---- */
void Vofa_StartFrame(void);              /* 清空帧缓冲，开始新一帧 */
void Vofa_SendData(uint8_t ch, float v); /* 追加一个通道（与原 vofa_send_data 一致） */
void Vofa_SendFrameTail(void);           /* 追加帧尾并整体发送（与原 vofa_sendframetail 一致） */
void Vofa_SendFrame(const float* values, uint8_t n); /* 一步封装：n 个通道写入后直接发送 */

/* ---- 下行：命令接收 ---- */
/* 喂入接收数据（放 DMA 空闲中断回调里，等价原 vofa_dma_recv / vofa_Receive）。 */
void Vofa_FeedRx(const uint8_t* buf, uint16_t len);

/* 工具：以 "key=value\r\n" 文本形式向主机回一条命令（如 "ack=1"）。 */
void Vofa_SendCmdText(const char* key, float value);

/* ============================================================
 * 使用示例（STM32 HAL + 无刷电机工程）
 * ============================================================
 * // 1) 底层发送（替换你自己的 vofa_transmit）
 * static void my_tx(const uint8_t* d, uint16_t n, void* u) {
 *     (void)u;
 *     memcpy(send_buf, d, n);                 // 自备缓冲供 DMA 使用
 *     HAL_UART_Transmit_DMA(&huart6, send_buf, n);
 * }
 *
 * // 2) 命令处理（对应原 vofa_Receive 里的 speed= / start= / stop= 等）
 * static void my_cmd(const char* key, float value, void* u) {
 *     (void)u;
 *     if      (strcmp(key, "speed") == 0)    pmsm_set_aimrpm((int16_t)value);
 *     else if (strcmp(key, "up_time") == 0)   pmsm_set_goup_down_time(1, (int16_t)value);
 *     else if (strcmp(key, "down_time") == 0) pmsm_set_goup_down_time(0, (int16_t)value);
 *     else if (strcmp(key, "start") == 0)  { if (value) pmsm_start_motor(); }
 *     else if (strcmp(key, "stop")  == 0)  { if (value) pmsm_stop_motor();  }
 * }
 *
 * // 3) 初始化
 * Vofa_Init(my_tx, my_cmd, NULL);
 *
 * // 4) 周期上传（等价原 vofa_start()）
 * Vofa_StartFrame();
 * Vofa_SendData(0, stcMotorCtrl.stcCurrent.Ia);
 * Vofa_SendData(1, stcMotorCtrl.stcCurrent.Ib);
 * Vofa_SendData(2, stcMotorCtrl.stcCurrent.Ic);
 * Vofa_SendData(3, stcMotorCtrl.SpeedRpmFlt);
 * Vofa_SendData(4, stcMotorCtrl.stcCurrent.Iq);
 * Vofa_SendFrameTail();
 *
 * // 5) 接收（放 DMA 空闲中断回调，等价原 vofa_dma_recv）
 * void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t size) {
 *     if (huart == &huart6) {
 *         Vofa_FeedRx(vofa_receive_buf, size);
 *         HAL_UARTEx_ReceiveToIdle_DMA(&huart6, vofa_receive_buf, RX_BUFFER_SIZE);
 *     }
 * }
 * ============================================================ */

#ifdef __cplusplus
}
#endif

#endif /* VOFA_COMMS_H */
