#pragma once
#include <QObject>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <optional>
#include "ParameterTypes.h"

namespace MotorStudio {

// 参数管理器
class ParameterManager : public QObject {
    Q_OBJECT
public:
    static ParameterManager& instance();
    ~ParameterManager();

    // 加载参数描述文件（JSON）
    bool loadDescription(const std::string& jsonFilePath);

    // 读取参数
    std::optional<ParamValue> read(uint16_t address);
    std::vector<ParamValue> readBatch(const std::vector<uint16_t>& addresses);

    // 写入参数
    bool write(uint16_t address, const ParamValue& value);
    bool writeBatch(const std::vector<std::pair<uint16_t, ParamValue>>& pairs);

    // 参数元数据
    const ParameterMeta* meta(uint16_t address) const;
    std::vector<const ParameterMeta*> metaByCategory(const std::string& category) const;
    std::vector<std::string> categories() const;

    // 导入/导出
    bool exportToFile(const std::string& filePath);
    bool importFromFile(const std::string& filePath);

    // 下载全部参数到设备
    void downloadAll();

    // 缓存管理
    void invalidateCache(uint16_t address);
    void invalidateAll();

    size_t parameterCount() const;

signals:
    void parameterChanged(const ParamChangeEvent& event);
    void descriptionLoaded(const std::string& filePath);
    void downloadProgress(int current, int total);

private:
    ParameterManager();
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace MotorStudio