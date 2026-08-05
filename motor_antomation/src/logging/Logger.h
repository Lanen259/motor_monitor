#pragma once
#include <QObject>
#include <string>
#include <memory>
#include <sstream>
#include <chrono>

namespace MotorStudio {

// 日志级别
enum class LogLevel : uint8_t {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

// 日志类别
enum class LogCategory : uint8_t {
    General,
    Protocol,
    Automation,
    Device,
    Curve,
    Performance,
    Plugin,
    Script,
    Communication,
    System
};

// 日志条目
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    LogCategory category;
    std::string message;
    std::string file;
    int line;
    std::string function;
};

// 日志管理器
class Logger : public QObject {
    Q_OBJECT
public:
    static Logger& instance();
    ~Logger() override;

    // 日志记录
    void log(LogLevel level, LogCategory category, const std::string& message,
             const char* file = nullptr, int line = 0, const char* function = nullptr);

    // 便捷方法
    void trace(LogCategory cat, const std::string& msg);
    void debug(LogCategory cat, const std::string& msg);
    void info(LogCategory cat, const std::string& msg);
    void warn(LogCategory cat, const std::string& msg);
    void error(LogCategory cat, const std::string& msg);
    void critical(LogCategory cat, const std::string& msg);

    // 级别过滤
    void setMinLevel(LogLevel level);
    LogLevel minLevel() const;

    // 类别过滤
    void enableCategory(LogCategory cat, bool enable = true);

    // 输出目标
    void enableConsoleOutput(bool enable);
    void enableFileOutput(const std::string& logDir, bool enable);

    // 查询
    std::vector<LogEntry> queryLogs(LogLevel minLevel, size_t maxEntries = 1000) const;

signals:
    void logEntryAdded(const LogEntry& entry);

private:
    Logger();
    struct Impl;
    std::unique_ptr<Impl> d;
};

// 日志宏
#define LOG_TRACE(cat, msg)    MotorStudio::Logger::instance().trace(cat, msg)
#define LOG_DEBUG(cat, msg)    MotorStudio::Logger::instance().debug(cat, msg)
#define LOG_INFO(cat, msg)     MotorStudio::Logger::instance().info(cat, msg)
#define LOG_WARN(cat, msg)     MotorStudio::Logger::instance().warn(cat, msg)
#define LOG_ERROR(cat, msg)    MotorStudio::Logger::instance().error(cat, msg)
#define LOG_CRITICAL(cat, msg) MotorStudio::Logger::instance().critical(cat, msg)

} // namespace MotorStudio