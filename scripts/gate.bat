@echo off
REM ============================================================
REM  Motor Studio 本地合并门禁（在仓库内"仿真"CI）
REM  作用：本地一键执行与 CI 相同的三步——
REM    1) qmake 全量构建（编译/链接检查）
REM    2) CMake 构建单元+集成测试
REM    3) ctest 跑全部测试
REM  用法：在 Windows cmd 里，仓库根目录执行：
REM        scripts\gate.bat
REM  可用环境变量覆盖工具链位置：
REM        GATE_QT_DIR      Qt 5.14.2 mingw73_32 根目录
REM                         （默认 D:/Program_Files/QT5.14/APP/5.14.2/mingw73_32）
REM        GATE_MINGW_BIN   MinGW 7.3 bin 目录
REM                         （默认 C:/MinGW/bin）
REM ============================================================

setlocal
set "REPO_ROOT=%~dp0.."
cd /d "%REPO_ROOT%"

if not defined GATE_QT_DIR set "GATE_QT_DIR=D:/Program_Files/QT5.14/APP/5.14.2/mingw73_32"
if not defined GATE_MINGW_BIN set "GATE_MINGW_BIN=C:/MinGW/bin"

if not exist "%GATE_MINGW_BIN%\g++.exe" goto :err_mingw
if not exist "%GATE_QT_DIR%\bin\qmake.exe" goto :err_qt
where cmake >nul 2>nul || goto :err_cmake

set "PATH=%GATE_MINGW_BIN%;%GATE_QT_DIR%\bin;%PATH%"
set "QT_QPA_PLATFORM=offscreen"

echo [GATE] ==== 1/3 qmake full build ====
pushd motor_antomation
qmake motor_antomation.pro
if errorlevel 1 goto :fail
mingw32-make -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 goto :fail
popd

echo [GATE] ==== 2/3 CMake test build ====
cmake -S motor_antomation -B build\ci-tests -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH="%GATE_QT_DIR%" -DCMAKE_C_COMPILER="%GATE_MINGW_BIN%\gcc.exe" -DCMAKE_CXX_COMPILER="%GATE_MINGW_BIN%\g++.exe" -DCMAKE_MAKE_PROGRAM="%GATE_MINGW_BIN%\mingw32-make.exe"
if errorlevel 1 goto :fail
cmake --build build\ci-tests -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 goto :fail

echo [GATE] ==== 3/3 run tests (ctest) ====
ctest --test-dir build\ci-tests --output-on-failure > build\ctest_log.txt 2>&1
set "CTEST_ERR=%errorlevel%"
type build\ctest_log.txt
if not "%CTEST_ERR%"=="0" goto :fail
REM 防御假阳性：ctest 找不到任何测试时退出码仍是 0，必须显式拦下
findstr /c:"No tests were found" build\ctest_log.txt >nul 2>nul && goto :fail_none

echo.
echo [GATE] PASSED: compile + all tests green, safe to merge to master.
exit /b 0

:fail
echo.
echo [GATE] FAILED: one or more steps failed. Fix and re-run.
exit /b 1

:fail_none
echo.
echo [GATE] FAILED: ctest reported "No tests were found" — test registration is broken.
exit /b 1

:err_mingw
echo [GATE] g++.exe not found: %GATE_MINGW_BIN%
echo        Set GATE_MINGW_BIN to your MinGW 7.3 bin directory.
exit /b 1

:err_qt
echo [GATE] qmake.exe not found: %GATE_QT_DIR%\bin
echo        Set GATE_QT_DIR to your Qt 5.14.2 mingw73_32 root.
exit /b 1

:err_cmake
echo [GATE] cmake not found. Install CMake and add it to PATH.
exit /b 1
