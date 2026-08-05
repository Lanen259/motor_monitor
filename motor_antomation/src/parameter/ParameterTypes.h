#pragma once
#include <variant>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace MotorStudio {

// 参数类型枚举
enum class ParamType : uint8_t {
    Bool,
    Int8, Int16, Int32, Int64,
    Uint8, Uint16, Uint32, Uint64,
    Float, Double,
    Enum,
    Bitfield,
    String
};

// 参数值（核心层使用 std::variant，UI 层转换为 QVariant）
using ParamValue = std::variant<
    bool,
    int8_t, int16_t, int32_t, int64_t,
    uint8_t, uint16_t, uint32_t, uint64_t,
    float, double,
    std::string
>;

// 枚举选项
struct EnumOption {
    int32_t value;
    std::string label;
};

// 参数元数据
struct ParameterMeta {
    uint16_t address;          // 参数地址
    std::string name;          // 参数名
    std::string description;   // 描述
    ParamType type;            // 类型
    ParamValue defaultValue;   // 默认值
    ParamValue minValue;       // 最小值
    ParamValue maxValue;       // 最大值
    std::string unit;          // 单位
    bool readOnly = false;     // 只读
    bool volatile_ = false;    // 易失（不持久化）
    std::vector<EnumOption> enumOptions; // 枚举选项
    std::string category;      // 分组
    uint8_t accessLevel = 0;   // 访问级别
};

// 参数变更事件
struct ParamChangeEvent {
    uint16_t address;
    ParamValue oldValue;
    ParamValue newValue;
    uint64_t timestampUs;
};

} // namespace MotorStudio