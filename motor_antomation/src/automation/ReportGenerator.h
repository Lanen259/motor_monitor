#pragma once
#include <QString>
#include <string>
#include "AutomationEngine.h"

namespace MotorStudio {

// 报告生成器 — 静态工具类，将 TestResult 输出为 HTML / CSV
class ReportGenerator {
public:
    // 生成 HTML 报告到 reports/ 目录
    // 返回生成的文件路径，失败返回空字符串
    static QString generateHtml(const TestResult& result, const TestCase& testCase,
                                const QString& reportsDir = "./reports");

    // 生成 CSV 报告到 reports/ 目录
    // 返回生成的文件路径，失败返回空字符串
    static QString generateCsv(const TestResult& result, const TestCase& testCase,
                               const QString& reportsDir = "./reports");

private:
    // 构建时间戳文件名前缀
    static QString buildTimestampPrefix();

    // 步骤类型转换
    static QString stepTypeToString(StepType type);

    // 从日志条目推断步骤状态: "Passed" | "Failed" | "Skipped"
    static QString stepStatus(const TestResult& result, int stepIndex);

    // HTML 转义
    static QString escapeHtml(const std::string& s);
    static QString escapeHtml(const QString& s);
};

} // namespace MotorStudio
