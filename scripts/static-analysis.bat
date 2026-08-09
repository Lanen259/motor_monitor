@echo off
REM ============================================================
REM  Motor Studio 静态分析门禁（clang-tidy + cppcheck）
REM  规则：零新增严重告警。既有告警记录在 reports\baseline_*.txt
REM  作为基线；只要求不新增。
REM
REM  用法：在 Windows cmd 里，仓库根目录执行：
REM        scripts\static-analysis.bat
REM
REM  前置条件：
REM    1) build\ci-tests\compile_commands.json 存在
REM       （先跑一次 scripts\gate.bat，其步骤 2 会生成它）
REM    2) cppcheck 与 clang-tidy 在 PATH 中
REM       （安装方式见 docs 规格文档 Phase 4）
REM
REM  输出：
REM    reports\cppcheck.txt        本次 cppcheck 全量结果
REM    reports\clang-tidy.txt      本次 clang-tidy 全量结果
REM    reports\baseline_*.txt      首次运行时固化的基线
REM  退出码：0 = 无新增严重告警；1 = 有新增严重告警或工具缺失
REM ============================================================

setlocal EnableDelayedExpansion
set "REPO_ROOT=%~dp0.."
cd /d "%REPO_ROOT%"

set "CC_JSON=build\ci-tests\compile_commands.json"
if not exist "%CC_JSON%" (
    echo [STATIC] 缺少 %CC_JSON%
    echo          请先运行 scripts\gate.bat 生成 compile_commands.json。
    exit /b 1
)

where cppcheck >nul 2>nul || goto :err_cppcheck
where clang-tidy >nul 2>nul || goto :err_tidy

if not exist reports mkdir reports

echo [STATIC] ==== 1/3 cppcheck ====
cppcheck --project="%CC_JSON%" ^
    --enable=warning,performance,portability ^
    --suppress=missingIncludeSystem ^
    --inline-suppressor ^
    --template="{file}:{line}: {severity}: {message} [{id}]" ^
    --output-file=reports\cppcheck.txt
if errorlevel 1 (
    echo [STATIC] cppcheck 执行异常，见 reports\cppcheck.txt
    exit /b 1
)
echo [STATIC] cppcheck 完成 -^> reports\cppcheck.txt

echo [STATIC] ==== 2/3 clang-tidy ====
if exist reports\clang-tidy.txt del reports\clang-tidy.txt
for /r "motor_antomation\src" %%f in (*.cpp) do (
    echo [STATIC]   clang-tidy %%f
    clang-tidy -p build\ci-tests "%%f" >> reports\clang-tidy.txt 2>&1
)
echo [STATIC] clang-tidy 完成 -^> reports\clang-tidy.txt

echo [STATIC] ==== 3/3 严重告警基线对比 ====
REM 提取严重级别行（cppcheck 的 error 严重度 + clang-tidy 的 error:）
findstr ": error:" reports\cppcheck.txt > reports\cppcheck_errors.tmp 2>nul || type nul > reports\cppcheck_errors.tmp
findstr /c:": error:" reports\clang-tidy.txt > reports\clangtidy_errors.tmp 2>nul || type nul > reports\clangtidy_errors.tmp

if not exist reports\baseline_cppcheck.txt (
    copy /y reports\cppcheck.txt reports\baseline_cppcheck.txt >nul
    copy /y reports\cppcheck_errors.tmp reports\baseline_cppcheck_errors.txt >nul
    copy /y reports\clang-tidy.txt reports\baseline_clang-tidy.txt >nul
    copy /y reports\clangtidy_errors.tmp reports\baseline_clangtidy_errors.txt >nul
    echo [STATIC] 首次运行：已创建告警基线 reports\baseline_*.txt
    echo [STATIC] PASSED（基线建立，后续运行将对比此基线）
    exit /b 0
)

REM 找出"不在基线里的严重告警行" = 新增严重告警
set "NEW_ERRORS=0"
findstr /v /x /l /g:reports\baseline_cppcheck_errors.txt reports\cppcheck_errors.tmp > reports\new_errors.tmp 2>nul
for %%A in (reports\new_errors.tmp) do if %%~zA GTR 0 (
    echo [STATIC] cppcheck 新增严重告警：
    type reports\new_errors.tmp
    set "NEW_ERRORS=1"
)
findstr /v /x /l /g:reports\baseline_clangtidy_errors.txt reports\clangtidy_errors.tmp > reports\new_errors.tmp 2>nul
for %%A in (reports\new_errors.tmp) do if %%~zA GTR 0 (
    echo [STATIC] clang-tidy 新增严重告警：
    type reports\new_errors.tmp
    set "NEW_ERRORS=1"
)

if "%NEW_ERRORS%"=="1" (
    echo.
    echo [STATIC] FAILED: 发现新增严重告警，请修复或经评审后更新基线。
    exit /b 1
)

echo [STATIC] PASSED: 零新增严重告警（与基线一致）。
exit /b 0

:err_cppcheck
echo [STATIC] cppcheck 未在 PATH 中找到。
echo          安装：https://cppcheck.sourceforge.io/ （安装时勾选 Add to PATH）
exit /b 1

:err_tidy
echo [STATIC] clang-tidy 未在 PATH 中找到。
echo          安装：LLVM for Windows https://github.com/llvm/llvm-project/releases （clang-tidy 包含在内）
exit /b 1
