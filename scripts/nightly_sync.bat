@echo off
rem ============================================================
rem 夜间任务一键同步脚本（在 Windows cmd 中运行）
rem 作用：把三个域分支夜间产出的提交同步到本地分支并推送；
rem       并把 master 合入各域分支（供下一轮使用）。
rem 本脚本只执行"同步/推送"这类安全操作；
rem 域分支 -> master 的合并请由集成代理在主 checkout 手动执行并验证。
rem ============================================================
cd /d E:\My_project\QT\Motor_Antomation || goto :fail

echo.
echo === 1. 抓取远程 ===
git fetch --all --prune || goto :fail

echo.
echo === 2. 推送三个域分支夜间提交 ===
git push origin domain/automation || goto :fail
git push origin domain/waveform   || goto :fail
git push origin domain/comms      || goto :fail

echo.
echo === 3. 各域分支合入最新 master（在各自 worktree 内执行）===
for %%w in (automation waveform comms) do (
    echo --- merge master into domain/%%w ---
    git -C E:\My_project\QT\Motor_Antomation-wt\%%w merge master || goto :fail
)

echo.
echo === 4. 查看三域夜间报告路径（人工验收用）===
echo 自动化: E:\My_project\QT\Motor_Antomation-wt\automation\reports\nightly_report_automation.md
echo 波形:   E:\My_project\QT\Motor_Antomation-wt\waveform\reports\nightly_report_waveform.md
echo 通讯:   E:\My_project\QT\Motor_Antomation-wt\comms\reports\nightly_report_comms.md

echo.
echo === 同步完成 ===
echo 下一步：由集成代理审阅三份报告，验收通过后在主 checkout 执行
echo   git merge domain/automation ^&^& scripts\gate.bat
echo   git merge domain/waveform   ^&^& scripts\gate.bat
echo   git merge domain/comms      ^&^& scripts\gate.bat
goto :eof

:fail
echo.
echo [失败] 同步未完成，请检查上方错误输出（若有合并冲突，按 CLAUDE.md 交由集成代理裁决）。
exit /b 1
