#include "Logger.h"

namespace MotorStudio {

struct Logger::Impl {
    LogLevel minLevel = LogLevel::Debug;
    bool consoleEnabled = true;
    bool fileEnabled = false;
};

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() : d(std::make_unique<Impl>()) {}
Logger::~Logger() = default;

void Logger::log(LogLevel level, LogCategory category, const std::string& message,
                 const char* file, int line, const char* function) {
    if (level < d->minLevel) return;
    // TODO: 异步写入
    LogEntry entry{std::chrono::system_clock::now(), level, category, message,
                   file ? file : "", line, function ? function : ""};
    emit logEntryAdded(entry);
}

void Logger::trace(LogCategory cat, const std::string& msg) { log(LogLevel::Trace, cat, msg); }
void Logger::debug(LogCategory cat, const std::string& msg) { log(LogLevel::Debug, cat, msg); }
void Logger::info(LogCategory cat, const std::string& msg)  { log(LogLevel::Info, cat, msg); }
void Logger::warn(LogCategory cat, const std::string& msg)  { log(LogLevel::Warning, cat, msg); }
void Logger::error(LogCategory cat, const std::string& msg) { log(LogLevel::Error, cat, msg); }
void Logger::critical(LogCategory cat, const std::string& msg) { log(LogLevel::Critical, cat, msg); }

void Logger::setMinLevel(LogLevel level) { d->minLevel = level; }
LogLevel Logger::minLevel() const { return d->minLevel; }

void Logger::enableCategory(LogCategory cat, bool enable) {}
void Logger::enableConsoleOutput(bool enable) { d->consoleEnabled = enable; }
void Logger::enableFileOutput(const std::string& logDir, bool enable) { d->fileEnabled = enable; }

std::vector<LogEntry> Logger::queryLogs(LogLevel minLevel, size_t maxEntries) const {
    return {};
}

} // namespace MotorStudio