#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace MotorStudio {

// ============================================================
// 帧格式定义
//
//  | Header | DevID | Cmd | Len | Payload | CRC16 | Footer |
//  | 2B     | 1B    | 1B  | 2B  | N bytes | 2B    | 2B     |
//
//  Header:  0xAA 0x55    同步头
//  DevID:   设备ID (0x01-0xFF)
//  Cmd:     命令类型
//  Len:     Payload 长度 (uint16 LE)
//  Payload: 数据载荷
//  CRC16:   CRC-16/XMODEM (uint16 LE)
//  Footer:  0x0D 0x0A    帧尾（可选，方便调试）
// ============================================================

// 帧常量
constexpr uint16_t FRAME_SYNC = 0x55AA;   // LE: AA 55
constexpr uint16_t FRAME_FOOTER = 0x0A0D; // LE: 0D 0A
constexpr size_t FRAME_HEADER_SIZE = 6;   // Sync(2) + DevID(1) + Cmd(1) + Len(2)
constexpr size_t FRAME_CRC_SIZE = 2;
constexpr size_t FRAME_FOOTER_SIZE = 2;
constexpr size_t FRAME_MIN_SIZE = FRAME_HEADER_SIZE + FRAME_CRC_SIZE + FRAME_FOOTER_SIZE; // 12

// 命令类型
enum class MotorCommand : uint8_t {
    DataUpload  = 0x01,  // 设备 -> 主机: 实时数据上传
    CmdRead     = 0x02,  // 主机 -> 设备: 读取参数
    CmdWrite    = 0x03,  // 主机 -> 设备: 写入参数
    Ack         = 0x04,  // 设备 -> 主机: 应答
    Nack        = 0x05,  // 设备 -> 主机: 否定应答
    Heartbeat   = 0x06,  // 设备 -> 主机: 心跳
    Reset       = 0x07,  // 主机 -> 设备: 复位
};

// 数据上传 Payload 结构 (packed, 无填充)
struct __attribute__((packed)) MotorDataPayload {
    uint32_t timestamp;    // 4 bytes, 毫秒时间戳
    float ia;              // 4 bytes, A相电流
    float ib;              // 4 bytes, B相电流
    float ic;              // 4 bytes, C相电流
    float id;              // 4 bytes, D轴电流
    float iq;              // 4 bytes, Q轴电流
    float speed;           // 4 bytes, 转速 (RPM)
    float position;        // 4 bytes, 位置 (电角度)
    float busVoltage;      // 4 bytes, 母线电压 (V)
    float busCurrent;      // 4 bytes, 母线电流 (A)
    float temperature;     // 4 bytes, 温度 (°C)
    uint16_t fault;        // 2 bytes, 故障码 (bitmask)
    // Expected: 4 + 11*4 + 2 = 50 bytes (packed);
    // Actual size depends on compiler, validated at runtime.
};

// 帧解析状态
enum class ParseState {
    WaitSync,
    WaitHeader,
    WaitPayload,
    WaitCrc,
    WaitFooter,
    Complete,
    Error,
};

// ============================================================
// CRC16 计算器
// ============================================================
class Crc16 {
public:
    // CRC-16/XMODEM (polynomial 0x1021)
    static uint16_t compute(const uint8_t* data, size_t length);
    static uint16_t compute(const std::vector<uint8_t>& data);
    static bool verify(const uint8_t* data, size_t length, uint16_t expectedCrc);

private:
    static uint16_t table_[256];
    static bool tableInitialized_;
    static void initTable();
};

// ============================================================
// 电机协议编解码器
// ============================================================
class MotorProtocol {
public:
    MotorProtocol();
    ~MotorProtocol();

    // --- 编码 ---

    // 编码实时数据为帧
    std::vector<uint8_t> encodeDataUpload(uint8_t deviceId, const MotorDataPayload& payload);

    // 编码命令帧
    std::vector<uint8_t> encodeCommand(uint8_t deviceId, MotorCommand cmd,
                                       const std::vector<uint8_t>& cmdPayload = {});

    // 编码原始帧
    std::vector<uint8_t> encodeFrame(uint8_t deviceId, MotorCommand cmd,
                                     const std::vector<uint8_t>& payload);

    // --- 解码 ---

    // 喂入原始字节，返回已解析的完整帧
    // 内部维护状态机，处理粘包/半包
    void feed(const std::vector<uint8_t>& rawData);

    // 是否有完整帧可用
    bool hasFrame() const;

    // 取出一帧（解码后的 payload）
    struct DecodedFrame {
        uint8_t deviceId;
        MotorCommand command;
        std::vector<uint8_t> payload;
        bool crcValid;
    };
    DecodedFrame popFrame();

    // 解析数据上传帧
    static bool parseDataPayload(const std::vector<uint8_t>& payload, MotorDataPayload& out);

    // 重置解析状态
    void reset();

    // 统计
    size_t framesDecoded() const { return framesDecoded_; }
    size_t crcErrors() const { return crcErrors_; }
    size_t frameErrors() const { return frameErrors_; }

private:
    ParseState state_;
    std::vector<uint8_t> buffer_;
    size_t expectedLength_;
    uint8_t currentDevId_;
    uint8_t currentCmd_;
    uint16_t currentPayloadLen_;
    std::vector<DecodedFrame> frameQueue_;

    size_t framesDecoded_;
    size_t crcErrors_;
    size_t frameErrors_;

    bool tryParseFrame();
};

} // namespace MotorStudio