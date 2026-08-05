# CHANGELOG.md — Motor Automation 变更记录

---

## [Unreleased] — 2026-08-05

### 需求访谈（六轮完成）
- 第0轮：产品定位 — 确定软件名称、用户画像、核心价值、竞品参考
- 第1轮：电机调试需求 — 电机类型(BLDC/PMSM)、控制方式(方波/FOC)、传感器(Hall)、三环控制、故障处理策略
- 第2轮：实时数据系统 — VOFA+协议、按波特率自适应、15分钟环形缓冲、多页面显示、CSV导出
- 第3轮：波形系统 + 自动化测试 — 自由运行无触发、内置计算引擎(RMS/峰值/Hall偏差)、表格配置测试流程、详细报告
- 第4轮：参数管理 + 通讯协议 — MCU自管参数、上位机只保存测试指令、VOFA+协议、命令集定义
- 第5轮：软件架构 + MVP + AI开发策略 — 工程文件JSON、多设备分页、日志策略、v0.1→v0.5→v1.0路线、多Agent自动开发

### 文档更新
- 新增 `motor_antomation_Software_Design_Document.md` 第0章：产品定义与需求规格（0.1~0.16）
- 文档版本升至 v1.3

### 项目管理文件
- 新增 `.agent/PROJECT_STATE.md` — 项目状态
- 新增 `.agent/TASK_BOARD.md` — 任务看板（v0.1 共 11 模块 35 任务）
- 新增 `.agent/CHANGELOG.md` — 本文件

---

## [Phase 2] — 2026-08-04

### 基础框架搭建
- 完成软件总体架构设计
- 搭建 Qt6 + CMake 项目骨架
- 实现多线程框架
- 实现 EventBus 事件总线
- 实现 MessageBus 消息总线

### 设计文档
- 编写 `motor_antomation_Software_Design_Document.md`（v1.0，13章+附录）
- 编写 `Analysis.md`（VESC Tool 深度分析）
- 编写 `docs/` 目录下各模块详细设计文档

---

## 格式说明

本文件遵循 [Keep a Changelog](https://keepachangelog.com/) 格式。

版本号遵循 [Semantic Versioning](https://semver.org/)。

变更类型：
- `Added` — 新增功能
- `Changed` — 功能变更
- `Deprecated` — 即将废弃
- `Removed` — 已移除
- `Fixed` — 修复
- `Security` — 安全修复