status: done
files_changed: [ReportGenerator.h, ReportGenerator.cpp, TestRunner.h, TestRunner.cpp]
evidence:
  - what_was_done: |
      1. ReportGenerator::generateHtml(result, testCase, reportsDir) — full HTML with embedded CSS,
         summary cards (Total/Pass/Fail/Skip), error banner, per-step table with colored rows,
         footer timestamp. Filename: report_YYYYMMDD_HHmmss.html
      2. ReportGenerator::generateCsv(result, testCase, reportsDir) — header row + one row per step
         with status parsed from TestResult.logs. Filename: report_YYYYMMDD_HHmmss.csv
      3. TestRunner stores currentTestCase, auto-generates reports on completion,
         emits reportGenerated(htmlPath, csvPath) signal
      4. Per-step status: cross-references TestResult.logs with TestCase.steps
         ([PASS]/[FAIL] prefix, beyond logs = Skipped)
risks:
  - Per-step duration not tracked ("N/A" in CSV) — would need AutomationEngine extension
decisions:
  - problem: TestResult lacks structured per-step data | chosen: Pass both TestResult + TestCase to ReportGenerator | reason: Avoids modifying AutomationEngine (out of scope); TestCase available in TestRunner
