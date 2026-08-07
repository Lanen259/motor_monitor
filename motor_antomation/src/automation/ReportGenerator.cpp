#include "ReportGenerator.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QDebug>

namespace MotorStudio {

QString ReportGenerator::buildTimestampPrefix()
{
    return "report_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
}

QString ReportGenerator::stepTypeToString(StepType type)
{
    switch (type) {
    case StepType::SetParameter:  return "SetParameter";
    case StepType::Wait:          return "Wait";
    case StepType::ReadParameter: return "ReadParameter";
    case StepType::Assert:        return "Assert";
    case StepType::RecordData:    return "RecordData";
    case StepType::StartMotor:    return "StartMotor";
    case StepType::StopMotor:     return "StopMotor";
    case StepType::Custom:        return "Custom";
    }
    return "Unknown";
}

QString ReportGenerator::stepStatus(const TestResult& result, int stepIndex)
{
    // Determine status for a given step index.
    // Steps at index < logs.size() were executed (Pass/Fail determined by log prefix).
    // Steps at index >= logs.size() were skipped (stopOnFailure or never reached).
    const int executedCount = static_cast<int>(result.logs.size());

    if (stepIndex < executedCount) {
        // Log entries are "[PASS] ..." or "[FAIL] ..."
        if (result.logs[stepIndex].rfind("[PASS]", 0) == 0) {
            return "Passed";
        }
        return "Failed";
    }
    return "Skipped";
}

QString ReportGenerator::escapeHtml(const std::string& s)
{
    return escapeHtml(QString::fromStdString(s));
}

QString ReportGenerator::escapeHtml(const QString& s)
{
    QString out = s;
    out.replace('&', "&amp;");
    out.replace('<', "&lt;");
    out.replace('>', "&gt;");
    out.replace('"', "&quot;");
    return out;
}

// ============================================================
// HTML Report
// ============================================================

QString ReportGenerator::generateHtml(const TestResult& result, const TestCase& testCase,
                                       const QString& reportsDir)
{
    QDir dir;
    if (!dir.mkpath(reportsDir)) {
        qWarning() << "ReportGenerator: Cannot create reports directory:" << reportsDir;
        return {};
    }

    const QString prefix = buildTimestampPrefix();
    const QString filePath = dir.filePath(reportsDir + "/" + prefix + ".html");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ReportGenerator: Cannot open file for writing:" << filePath;
        return {};
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    // --- HTML Header ---
    out << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "  <meta charset=\"UTF-8\">\n"
        << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        << "  <title>Test Report — " << escapeHtml(result.caseName) << "</title>\n"
        << "  <style>\n"
        << "    * { margin: 0; padding: 0; box-sizing: border-box; }\n"
        << "    body {\n"
        << "      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n"
        << "      background: #f5f6fa; color: #2d3436;\n"
        << "      padding: 30px;\n"
        << "    }\n"
        << "    .container { max-width: 960px; margin: 0 auto; background: #fff; border-radius: 8px; box-shadow: 0 2px 12px rgba(0,0,0,0.08); overflow: hidden; }\n"
        << "    .header {\n"
        << "      padding: 25px 30px;\n"
        << "      border-bottom: 3px solid #dfe6e9;\n"
        << "    }\n"
        << "    .header h1 { font-size: 24px; margin-bottom: 6px; }\n"
        << "    .header .subtitle { color: #636e72; font-size: 14px; }\n"
        << "    .summary {\n"
        << "      display: flex; gap: 20px; padding: 20px 30px;\n"
        << "      background: #fafafa; border-bottom: 1px solid #dfe6e9;\n"
        << "    }\n"
        << "    .summary-card {\n"
        << "      flex: 1; text-align: center; padding: 14px 8px;\n"
        << "      border-radius: 6px; background: #fff; box-shadow: 0 1px 4px rgba(0,0,0,0.05);\n"
        << "    }\n"
        << "    .summary-card .label { font-size: 12px; text-transform: uppercase; color: #636e72; margin-bottom: 4px; }\n"
        << "    .summary-card .value { font-size: 28px; font-weight: 700; }\n"
        << "    .card-total  .value { color: #0984e3; }\n"
        << "    .card-pass   .value { color: #00b894; }\n"
        << "    .card-fail   .value { color: #d63031; }\n"
        << "    .card-skip   .value { color: #b2bec3; }\n"
        << "    .duration    { padding: 10px 30px; font-size: 13px; color: #636e72; background: #fafafa; border-bottom: 1px solid #dfe6e9; }\n"
        << "    .error-msg {\n"
        << "      margin: 15px 30px; padding: 12px 16px;\n"
        << "      background: #ffeaa7; border-left: 4px solid #fdcb6e;\n"
        << "      border-radius: 4px; font-size: 14px; display: none;\n"
        << "    }\n"
        << "    .error-msg.visible { display: block; }\n"
        << "    table {\n"
        << "      width: 100%; border-collapse: collapse;\n"
        << "    }\n"
        << "    thead th {\n"
        << "      text-align: left; padding: 12px 16px;\n"
        << "      background: #dfe6e9; font-size: 12px;\n"
        << "      text-transform: uppercase; letter-spacing: 0.5px; color: #2d3436;\n"
        << "      border-bottom: 2px solid #b2bec3;\n"
        << "    }\n"
        << "    tbody td {\n"
        << "      padding: 10px 16px; font-size: 14px;\n"
        << "      border-bottom: 1px solid #ecf0f1;\n"
        << "    }\n"
        << "    tr.pass  td { background: #e6faf2; }\n"
        << "    tr.fail  td { background: #fde8e8; }\n"
        << "    tr.skip  td { background: #f1f2f6; color: #b2bec3; }\n"
        << "    .badge {\n"
        << "      display: inline-block; padding: 2px 10px;\n"
        << "      border-radius: 12px; font-size: 11px; font-weight: 600; text-transform: uppercase;\n"
        << "      color: #fff;\n"
        << "    }\n"
        << "    .badge-pass { background: #00b894; }\n"
        << "    .badge-fail { background: #d63031; }\n"
        << "    .badge-skip { background: #b2bec3; }\n"
        << "    .footer {\n"
        << "      padding: 16px 30px; font-size: 12px; color: #b2bec3;\n"
        << "      text-align: center; border-top: 1px solid #dfe6e9;\n"
        << "    }\n"
        << "  </style>\n"
        << "</head>\n"
        << "<body>\n";

    // --- Header ---
    out << "<div class=\"container\">\n";

    out << "  <div class=\"header\">\n"
        << "    <h1>" << escapeHtml(result.caseName) << "</h1>\n"
        << "    <div class=\"subtitle\">"
        << escapeHtml(testCase.description) << "</div>\n"
        << "  </div>\n";

    // --- Summary card ---
    const int totalSteps = static_cast<int>(testCase.steps.size());
    const int executedCount = static_cast<int>(result.logs.size());

    int passCount = 0;
    int failCount = 0;
    for (int i = 0; i < executedCount; ++i) {
        if (result.logs[i].rfind("[PASS]", 0) == 0)
            ++passCount;
        else
            ++failCount;
    }
    const int skipCount = totalSteps - executedCount;

    out << "  <div class=\"summary\">\n"
        << "    <div class=\"summary-card card-total\">\n"
        << "      <div class=\"label\">Total</div>\n"
        << "      <div class=\"value\">" << totalSteps << "</div>\n"
        << "    </div>\n"
        << "    <div class=\"summary-card card-pass\">\n"
        << "      <div class=\"label\">Pass</div>\n"
        << "      <div class=\"value\">" << passCount << "</div>\n"
        << "    </div>\n"
        << "    <div class=\"summary-card card-fail\">\n"
        << "      <div class=\"label\">Fail</div>\n"
        << "      <div class=\"value\">" << failCount << "</div>\n"
        << "    </div>\n"
        << "    <div class=\"summary-card card-skip\">\n"
        << "      <div class=\"label\">Skip</div>\n"
        << "      <div class=\"value\">" << skipCount << "</div>\n"
        << "    </div>\n"
        << "  </div>\n";

    // --- Duration ---
    out << "  <div class=\"duration\">\n"
        << "    Duration: " << result.duration.count() << " ms\n"
        << "  </div>\n";

    // --- Error message (if failed) ---
    if (!result.passed && !result.errorMessage.empty()) {
        out << "  <div class=\"error-msg visible\">\n"
            << "    <strong>Error:</strong> " << escapeHtml(result.errorMessage) << "\n"
            << "  </div>\n";
    } else {
        out << "  <div class=\"error-msg\"></div>\n";
    }

    // --- Step table ---
    out << "  <table>\n"
        << "    <thead>\n"
        << "      <tr>\n"
        << "        <th>Step #</th>\n"
        << "        <th>Type</th>\n"
        << "        <th>Description</th>\n"
        << "        <th>Status</th>\n"
        << "        <th>Log</th>\n"
        << "      </tr>\n"
        << "    </thead>\n"
        << "    <tbody>\n";

    for (int i = 0; i < totalSteps; ++i) {
        const TestStep& step = testCase.steps[i];
        QString status = stepStatus(result, i);
        QString rowClass;
        QString badgeClass;
        if (status == "Passed") {
            rowClass = "pass";
            badgeClass = "badge-pass";
        } else if (status == "Failed") {
            rowClass = "fail";
            badgeClass = "badge-fail";
        } else {
            rowClass = "skip";
            badgeClass = "badge-skip";
        }

        QString logEntry = (i < executedCount)
            ? escapeHtml(QString::fromStdString(result.logs[i]))
            : QString();

        out << "      <tr class=\"" << rowClass << "\">\n"
            << "        <td>" << (i + 1) << "</td>\n"
            << "        <td>" << escapeHtml(stepTypeToString(step.type)) << "</td>\n"
            << "        <td>" << escapeHtml(step.description) << "</td>\n"
            << "        <td><span class=\"badge " << badgeClass << "\">" << status << "</span></td>\n"
            << "        <td>" << logEntry << "</td>\n"
            << "      </tr>\n";
    }

    out << "    </tbody>\n"
        << "  </table>\n";

    // --- Footer ---
    QString reportTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    out << "  <div class=\"footer\">\n"
        << "    Generated by Motor Automation Studio — " << reportTime << "\n"
        << "  </div>\n";

    out << "</div>\n"  // .container
        << "</body>\n"
        << "</html>\n";

    file.close();
    qDebug() << "ReportGenerator: HTML report saved to" << filePath;
    return QFileInfo(file).absoluteFilePath();
}

// ============================================================
// CSV Report
// ============================================================

QString ReportGenerator::generateCsv(const TestResult& result, const TestCase& testCase,
                                      const QString& reportsDir)
{
    QDir dir;
    if (!dir.mkpath(reportsDir)) {
        qWarning() << "ReportGenerator: Cannot create reports directory:" << reportsDir;
        return {};
    }

    const QString prefix = buildTimestampPrefix();
    const QString filePath = dir.filePath(reportsDir + "/" + prefix + ".csv");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "ReportGenerator: Cannot open file for writing:" << filePath;
        return {};
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    // Helper: quote a CSV field (escape double quotes, wrap in double quotes)
    auto csvField = [](const QString& s) -> QString {
        QString escaped = s;
        escaped.replace('"', "\"\"");
        return '"' + escaped + '"';
    };

    // Header row
    out << "Step#,Type,Description,Status,DurationMs,ErrorMsg\n";

    const int totalSteps = static_cast<int>(testCase.steps.size());
    const int executedCount = static_cast<int>(result.logs.size());

    for (int i = 0; i < totalSteps; ++i) {
        const TestStep& step = testCase.steps[i];
        QString status = stepStatus(result, i);
        QString description = QString::fromStdString(step.description);
        QString errorMsg;

        if (status == "Failed") {
            // For the failed step, include error details from the log
            if (i < executedCount) {
                errorMsg = QString::fromStdString(result.logs[i]);
            }
            if (i == result.failedStepIndex && !result.errorMessage.empty()) {
                if (!errorMsg.isEmpty()) errorMsg += "; ";
                errorMsg += QString::fromStdString(result.errorMessage);
            }
        }

        out << (i + 1) << ","
            << csvField(stepTypeToString(step.type)) << ","
            << csvField(description) << ","
            << csvField(status) << ","
            << "N/A" << ","                          // per-step duration not tracked
            << csvField(errorMsg) << "\n";
    }

    file.close();
    qDebug() << "ReportGenerator: CSV report saved to" << filePath;
    return QFileInfo(file).absoluteFilePath();
}

} // namespace MotorStudio
