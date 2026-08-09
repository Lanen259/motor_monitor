@echo off
REM ============================================================
REM  Motor Studio static analysis gate (clang-tidy + cppcheck)
REM  Rule: zero NEW critical warnings. Existing warnings are kept
REM  in reports\baseline_*.txt as the baseline.
REM
REM  Usage (run from repo root):
REM        scripts\static-analysis.bat
REM
REM  Prerequisites:
REM    1) build\ci-tests\compile_commands.json exists
REM       (run scripts\gate.bat once - step 2 generates it)
REM    2) cppcheck and clang-tidy are available.
REM       Optional: set CPPCHECK_BIN / CLANG_TIDY_BIN to the
REM       directory containing the tools (else PATH is searched).
REM       clang-tidy can reuse the one bundled with Qt Creator:
REM       D:/Program_flies/qt_creat/APP/Tools/QtCreator/bin/clang/bin
REM
REM  Outputs:
REM    reports\cppcheck.txt      this run's full cppcheck result
REM    reports\clang-tidy.txt    this run's full clang-tidy result
REM    reports\baseline_*.txt    baseline frozen on first run
REM  Exit code: 0 = no new critical warnings; 1 = new critical
REM  warnings found or a tool is missing
REM ============================================================

setlocal EnableDelayedExpansion
set "REPO_ROOT=%~dp0.."
cd /d "%REPO_ROOT%"

set "CC_JSON=build\ci-tests\compile_commands.json"
if not exist "%CC_JSON%" (
    echo [STATIC] missing %CC_JSON%
    echo           run scripts\gate.bat first to generate compile_commands.json.
    exit /b 1
)

REM Locate tools: honor CPPCHECK_BIN / CLANG_TIDY_BIN overrides, else PATH.
if defined CPPCHECK_BIN (
    if not exist "%CPPCHECK_BIN%\cppcheck.exe" goto :err_cppcheck
    set "CPPCHECK_CMD=%CPPCHECK_BIN%\cppcheck.exe"
) else (
    where cppcheck >nul 2>nul || goto :err_cppcheck
    set "CPPCHECK_CMD=cppcheck"
)
if defined CLANG_TIDY_BIN (
    if not exist "%CLANG_TIDY_BIN%\clang-tidy.exe" goto :err_tidy
    set "CLANG_TIDY_CMD=%CLANG_TIDY_BIN%\clang-tidy.exe"
) else (
    where clang-tidy >nul 2>nul || goto :err_tidy
    set "CLANG_TIDY_CMD=clang-tidy"
)

if not exist reports mkdir reports

echo [STATIC] ==== 1/3 cppcheck ====
"%CPPCHECK_CMD%" --project="%CC_JSON%" ^
    --enable=warning,performance,portability ^
    --suppress=missingIncludeSystem ^
    --inline-suppr ^
    --library=qt ^
    -i build ^
    --template="{file}:{line}: {severity}: {message} [{id}]" ^
    --output-file=reports\cppcheck.txt
if errorlevel 1 (
    echo [STATIC] cppcheck failed, see reports\cppcheck.txt
    exit /b 1
)
echo [STATIC] cppcheck done -^> reports\cppcheck.txt

echo [STATIC] ==== 2/3 clang-tidy ====
if exist reports\clang-tidy.txt del reports\clang-tidy.txt
REM 用 compile_commands.json 里的绝对源码路径驱动 clang-tidy：
REM for /r 的相对路径无法被 -p 正确匹配到编译命令。
python -c "import json,sys; [print(e['file']) for e in json.load(open(sys.argv[1],encoding='utf-8')) if e['file'].find('motor_antomation/src/')>=0 and not e['file'].find('/autogen/')>=0]" "%CC_JSON%" > %TEMP%\tidy_files.txt
for /f "usebackq delims=" %%f in ("%TEMP%\tidy_files.txt") do (
    echo [STATIC]   clang-tidy %%f
    "%CLANG_TIDY_CMD%" -p build\ci-tests "%%f" >> reports\clang-tidy.txt 2>&1
)
echo [STATIC] clang-tidy done -^> reports\clang-tidy.txt

echo [STATIC] ==== 3/3 compare critical warnings vs baseline ====
findstr /c:": error:" reports\cppcheck.txt > reports\cppcheck_errors.tmp 2>nul || type nul > reports\cppcheck_errors.tmp
findstr /c:": error:" reports\clang-tidy.txt > reports\clangtidy_errors.tmp 2>nul || type nul > reports\clangtidy_errors.tmp

if not exist reports\baseline_cppcheck.txt (
    copy /y reports\cppcheck.txt reports\baseline_cppcheck.txt >nul
    copy /y reports\cppcheck_errors.tmp reports\baseline_cppcheck_errors.txt >nul
    copy /y reports\clang-tidy.txt reports\baseline_clang-tidy.txt >nul
    copy /y reports\clangtidy_errors.tmp reports\baseline_clangtidy_errors.txt >nul
    echo [STATIC] first run: baseline written to reports\baseline_*.txt
    echo [STATIC] PASSED baseline established; later runs compare against it
    exit /b 0
)

REM New critical warnings = error lines not present in the baseline.
REM NOTE: findstr /g: with an EMPTY baseline file is unreliable (returns 2),
REM so guard the baseline-empty case explicitly.
set "NEW_ERRORS=0"

for %%B in (reports\baseline_cppcheck_errors.txt) do set "BASE_CP=%%~zB"
for %%A in (reports\cppcheck_errors.tmp) do set "CUR_CP=%%~zA"
if "%BASE_CP%"=="0" (
    if not "%CUR_CP%"=="0" (
        echo [STATIC] new cppcheck critical warnings:
        type reports\cppcheck_errors.tmp
        set "NEW_ERRORS=1"
    )
) else (
    findstr /v /x /l /g:reports\baseline_cppcheck_errors.txt reports\cppcheck_errors.tmp > reports\new_errors.tmp 2>nul
    for %%A in (reports\new_errors.tmp) do if %%~zA GTR 0 (
        echo [STATIC] new cppcheck critical warnings:
        type reports\new_errors.tmp
        set "NEW_ERRORS=1"
    )
)

for %%B in (reports\baseline_clangtidy_errors.txt) do set "BASE_CT=%%~zB"
for %%A in (reports\clangtidy_errors.tmp) do set "CUR_CT=%%~zA"
if "%BASE_CT%"=="0" (
    if not "%CUR_CT%"=="0" (
        echo [STATIC] new clang-tidy critical warnings:
        type reports\clangtidy_errors.tmp
        set "NEW_ERRORS=1"
    )
) else (
    findstr /v /x /l /g:reports\baseline_clangtidy_errors.txt reports\clangtidy_errors.tmp > reports\new_errors.tmp 2>nul
    for %%A in (reports\new_errors.tmp) do if %%~zA GTR 0 (
        echo [STATIC] new clang-tidy critical warnings:
        type reports\new_errors.tmp
        set "NEW_ERRORS=1"
    )
)

if "%NEW_ERRORS%"=="1" (
    echo.
    echo [STATIC] FAILED: new critical warnings found. Fix them or update the baseline after review.
    exit /b 1
)

echo [STATIC] PASSED: zero new critical warnings (matches baseline).
exit /b 0

:err_cppcheck
echo [STATIC] cppcheck not found.
echo          Install: https://cppcheck.sourceforge.io/  (add to PATH), or set CPPCHECK_BIN.
exit /b 1

:err_tidy
echo [STATIC] clang-tidy not found.
echo          Install LLVM for Windows: https://github.com/llvm/llvm-project/releases
echo          or reuse the Qt Creator bundled one and set CLANG_TIDY_BIN.
exit /b 1
