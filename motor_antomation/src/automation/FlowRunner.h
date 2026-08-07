#pragma once

#include <QObject>
#include <atomic>
#include <functional>
#include <chrono>
#include <string>
#include <vector>

#include "ExpressionEngine.h"  // for ValueProvider

namespace MotorStudio {

// Forward declarations
struct FlowGraph;
struct FlowNode;
class VariableScope;
class AutomationEngine;

/// Execution context passed through the entire flow run.
struct ExecutionContext {
    VariableScope* variables = nullptr;                // variable table (may be shared across sub-flows)
    AutomationEngine* engine = nullptr;                // for setParam/readParam/motor control
    std::function<void(const std::string&)> log;       // log callback
    bool stopRequested = false;
    bool pauseRequested = false;
    std::chrono::milliseconds totalTimeout{0};         // 0 = no limit
    int maxSteps = 10000;                              // hard limit protection
};

/// Result of a single node execution.
struct FlowStepResult {
    std::string nodeId;
    bool passed = true;
    std::string errorMessage;
    std::chrono::milliseconds duration{0};
    std::string logLine;
};

/// Top-level result returned after a flow graph run completes.
struct FlowRunResult {
    bool passed = false;
    bool stopped = false;       // user requested stop
    bool timeout = false;       // total timeout exceeded
    std::string errorMessage;
    std::vector<FlowStepResult> stepResults;
    std::chrono::milliseconds totalDuration{0};
};

/// Execution engine that walks a FlowGraph node-by-node.
///
/// Supports branching (If), looping (Loop), sub-flows (SubFlow),
/// pause/stop, step limits, and total timeout.
class FlowRunner : public QObject {
    Q_OBJECT
public:
    explicit FlowRunner(AutomationEngine* engine, QObject* parent = nullptr);

    /// Start executing the graph.  This call blocks until the run completes.
    void run(const FlowGraph& graph, ExecutionContext ctx);

    /// Request a graceful stop at the next node boundary.
    void stop();

    /// Request a pause.
    void pause();

    /// Resume from a pause.
    void resume();

signals:
    void nodeStarted(const std::string& nodeId);
    void nodeCompleted(const std::string& nodeId, bool success, const std::string& error);
    void runnerFinished(const FlowRunResult& result);
    void logMessage(const std::string& message);

private:
    // ---- Top-level orchestration ----------------------------------------------
    FlowRunResult executeGraph(const FlowGraph& graph, ExecutionContext& ctx,
                               VariableScope* localScope = nullptr);

    // ---- Single-node dispatch -------------------------------------------------
    FlowStepResult executeNode(const FlowNode& node, ExecutionContext& ctx,
                               const FlowGraph& parentGraph);

    // ---- Per-node-type executors ----------------------------------------------
    FlowStepResult execSetParameter(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execDelay(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execWaitCondition(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execReadParameter(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execStartMotor(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execStopMotor(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execAssign(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execCalc(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execIf(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execLoop(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execSubFlow(const FlowNode& node, ExecutionContext& ctx,
                               const FlowGraph& parentGraph);
    FlowStepResult execAssert(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execLog(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execRecordData(const FlowNode& node, ExecutionContext& ctx);
    FlowStepResult execUnsupported(const FlowNode& node, ExecutionContext& ctx,
                                   const std::string& type);

    // ---- Helpers --------------------------------------------------------------
    static ValueProvider makeValueProvider(VariableScope* variables);
    static bool evalCondition(const FlowNode& node, ExecutionContext& ctx,
                              const std::string& key = "condition");
    static std::string paramValue(const FlowNode& node, const std::string& key,
                                  const std::string& defaultValue = {});
    void handlePause(ExecutionContext& ctx);

    AutomationEngine* m_engine;

    // 跨线程的停止/暂停标志：FlowRunner 运行在工作线程，
    // stop()/pause()/resume() 由 UI 线程调用，用原子变量传递状态。
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_pauseRequested{false};
};

} // namespace MotorStudio
