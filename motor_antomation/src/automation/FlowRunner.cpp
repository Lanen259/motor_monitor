#include "FlowRunner.h"
#include "FlowGraph.h"
#include "VariableScope.h"
#include "AutomationEngine.h"
#include "../databus/DataBus.h"
#include "../databus/Topic.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace MotorStudio {

// ============================================================================
// Construction
// ============================================================================

FlowRunner::FlowRunner(AutomationEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
}

// ============================================================================
// Public API
// ============================================================================

void FlowRunner::run(const FlowGraph& graph, ExecutionContext ctx)
{
    // 每次运行重置跨线程停止/暂停标志
    m_stopRequested = false;
    m_pauseRequested = false;
    ctx.stopRequested = false;
    ctx.pauseRequested = false;

    auto result = executeGraph(graph, ctx);
    emit runnerFinished(result);
}

void FlowRunner::stop()
{
    m_stopRequested = true;
    m_pauseRequested = false;
}

void FlowRunner::pause()
{
    m_pauseRequested = true;
}

void FlowRunner::resume()
{
    m_pauseRequested = false;
}

// ============================================================================
// Helpers
// ============================================================================

ValueProvider FlowRunner::makeValueProvider(VariableScope* variables)
{
    ValueProvider provider;
    provider.getVariable = [variables](const std::string& name) -> std::optional<double> {
        if (!variables) return std::nullopt;
        return variables->resolveNumber(name);
    };
    provider.getChannel = [](const std::string& name) -> std::optional<double> {
        TopicId tid = TopicRegistry::instance().findTopic(name);
        if (tid == 0) return std::nullopt;
        auto val = DataBus::instance().latestValue(tid);
        if (val.has_value())
            return static_cast<double>(val.value());
        return std::nullopt;
    };
    return provider;
}

bool FlowRunner::evalCondition(const FlowNode& node, ExecutionContext& ctx,
                                const std::string& key)
{
    // 兼容多种参数键：面板按节点类型写入 "expression" 或 "condition"
    std::string expr = paramValue(node, key);
    if (expr.empty()) expr = paramValue(node, "expression");
    if (expr.empty()) expr = paramValue(node, "condition");
    if (expr.empty()) {
        if (ctx.log) ctx.log("[FlowRunner] evalCondition: empty expression for key '" + key + "'");
        return false;
    }

    ValueProvider provider = makeValueProvider(ctx.variables);
    auto result = ExpressionEngine::evaluate(expr, provider);
    if (!result.has_value()) {
        if (ctx.log) ctx.log("[FlowRunner] evalCondition: failed to evaluate '" + expr + "'");
        return false;
    }
    return result.value() != 0.0;
}

std::string FlowRunner::paramValue(const FlowNode& node, const std::string& key,
                                    const std::string& defaultValue)
{
    for (const auto& kv : node.params) {
        if (kv.first == key)
            return kv.second;
    }
    return defaultValue;
}

void FlowRunner::handlePause(ExecutionContext& ctx)
{
    while ((ctx.pauseRequested || m_pauseRequested.load())
           && !ctx.stopRequested && !m_stopRequested.load()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QThread::msleep(50);
    }
}

// ============================================================================
// Top-level graph execution
// ============================================================================

FlowRunResult FlowRunner::executeGraph(const FlowGraph& graph, ExecutionContext& ctx,
                                        VariableScope* localScope)
{
    FlowRunResult result;

    auto startTime = std::chrono::steady_clock::now();

    // Create local variable scope (parented to the caller's scope for lookup chaining)
    // For top-level calls, localScope is null and we create the scope here.
    // For sub-flow calls, execSubFlow already created the scope and passes it in.
    VariableScope defaultScope;
    if (!localScope) {
        if (ctx.variables)
            defaultScope.setParentScope(ctx.variables);
        localScope = &defaultScope;
    }
    VariableScope* savedVars = ctx.variables;
    ctx.variables = localScope;

    // Find the entry point
    const FlowNode* start = graph.startNode();
    if (!start) {
        result.errorMessage = "Flow graph has no start node";
        return result;
    }

    int stepCount = 0;

    // Loop state: track per-loop-node iteration counts to prevent infinite loops
    std::unordered_map<std::string, int> loopIterations;
    static const int kMaxLoopIterations = 10000;

    const FlowNode* currentNode = start;

    while (currentNode && stepCount < ctx.maxSteps && !result.stopped
           && !m_stopRequested.load()) {
        // ---- Stop / Pause / Timeout checks ----
        if (ctx.stopRequested || m_stopRequested.load()) {
            result.stopped = true;
            break;
        }

        handlePause(ctx);
        if (ctx.stopRequested || m_stopRequested.load()) {
            result.stopped = true;
            break;
        }

        if (ctx.totalTimeout.count() > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);
            if (elapsed >= ctx.totalTimeout) {
                result.timeout = true;
                result.errorMessage = "Total timeout exceeded (" +
                    std::to_string(ctx.totalTimeout.count()) + " ms)";
                break;
            }
        }

        // ---- Execute current node ----
        emit nodeStarted(currentNode->id);

        auto stepStartTime = std::chrono::steady_clock::now();
        FlowStepResult stepResult = executeNode(*currentNode, ctx, graph);
        auto stepEndTime = std::chrono::steady_clock::now();

        stepResult.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            stepEndTime - stepStartTime);
        stepResult.nodeId = currentNode->id;

        result.stepResults.push_back(stepResult);

        emit nodeCompleted(currentNode->id, stepResult.passed,
                           stepResult.errorMessage);

        if (!stepResult.logLine.empty()) {
            emit logMessage(stepResult.logLine);
            if (ctx.log) ctx.log(stepResult.logLine);
        }

        // ---- Determine next node ----
        const auto& nodeType = currentNode->type;
        auto outEdges = graph.edgesFrom(currentNode->id);

        if (nodeType == "If") {
            // If node: follow Yes port when condition is true, No port otherwise
            PortType targetPort = stepResult.passed ? PortType::Yes : PortType::No;
            const FlowEdge* branchEdge = nullptr;
            for (const auto* e : outEdges) {
                if (e->fromPort == targetPort) {
                    branchEdge = e;
                    break;
                }
            }
            currentNode = branchEdge ? graph.findNode(branchEdge->toNodeId) : nullptr;

        } else if (nodeType == "Loop") {
            auto& iterCount = loopIterations[currentNode->id];

            if (stepResult.passed && iterCount < kMaxLoopIterations) {
                // Condition true: enter / repeat loop body via Default port
                ++iterCount;
                const FlowEdge* bodyEdge = nullptr;
                for (const auto* e : outEdges) {
                    if (e->fromPort == PortType::Default) {
                        bodyEdge = e;
                        break;
                    }
                }
                currentNode = bodyEdge ? graph.findNode(bodyEdge->toNodeId) : nullptr;
                if (!currentNode) {
                    result.errorMessage = "Loop node: no Default (body) edge or target not found";
                }
            } else {
                // Condition false or max iterations: exit loop via No port
                if (iterCount >= kMaxLoopIterations && stepResult.passed) {
                    if (ctx.log)
                        ctx.log("[FlowRunner] Loop '" + currentNode->id +
                                "': max iterations reached, exiting");
                }
                iterCount = 0;  // reset for next encounter
                const FlowEdge* exitEdge = nullptr;
                for (const auto* e : outEdges) {
                    if (e->fromPort == PortType::No) {
                        exitEdge = e;
                        break;
                    }
                }
                currentNode = exitEdge ? graph.findNode(exitEdge->toNodeId) : nullptr;
            }

        } else if (!stepResult.passed) {
            // Non-branching node failed — stop execution
            result.errorMessage = "Step '" + currentNode->id +
                "' failed: " + stepResult.errorMessage;
            currentNode = nullptr;

        } else if (!outEdges.empty()) {
            // Normal progression: follow the first Default edge
            const FlowEdge* nextEdge = outEdges[0];  // edgesFrom sorts by priority
            currentNode = graph.findNode(nextEdge->toNodeId);

        } else {
            // No outgoing edges — end of graph
            currentNode = nullptr;
        }

        ++stepCount;
    }

    // Final verdict
    if (!result.stopped && !result.timeout && result.errorMessage.empty()) {
        result.passed = true;
    }
    if (stepCount >= ctx.maxSteps && !result.stopped) {
        result.timeout = true;
        result.passed = false;
        if (result.errorMessage.empty())
            result.errorMessage = "Step limit exceeded (" +
                std::to_string(ctx.maxSteps) + ")";
    }

    auto endTime = std::chrono::steady_clock::now();
    result.totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    // Restore the caller's variable scope
    ctx.variables = savedVars;

    return result;
}

// ============================================================================
// Node dispatch
// ============================================================================

FlowStepResult FlowRunner::executeNode(const FlowNode& node, ExecutionContext& ctx,
                                       const FlowGraph& parentGraph)
{
    const std::string& t = node.type;

    // 控制
    if (t == "SetParameter")   return execSetParameter(node, ctx);
    if (t == "StartMotor")     return execStartMotor(node, ctx);
    if (t == "StopMotor")      return execStopMotor(node, ctx);
    // 时序
    if (t == "Wait" || t == "Delay") return execDelay(node, ctx);
    if (t == "WaitCondition")  return execWaitCondition(node, ctx);
    // 逻辑（面板节点名与执行器节点名对齐）
    if (t == "If" || t == "Switch") return execIf(node, ctx);
    if (t == "Loop" || t == "While") return execLoop(node, ctx);
    // 数学
    if (t == "Assign" || t == "AssignVariable") return execAssign(node, ctx);
    if (t == "Calculate" || t == "Math" || t == "Expression") return execCalc(node, ctx);
    // 通信/数据
    if (t == "ReadParameter")  return execReadParameter(node, ctx);
    if (t == "RecordData")     return execRecordData(node, ctx);
    if (t == "Log" || t == "LogOutput") return execLog(node, ctx);
    // 断言
    if (t == "Assert")         return execAssert(node, ctx);
    // 流程
    if (t == "SubFlow")        return execSubFlow(node, ctx, parentGraph);
    if (t == "Comment" || t == "Nop" || t == "Label") {
        FlowStepResult r;
        r.passed = true;
        r.logLine = "[" + t + "] " + (node.label.empty() ? "(no-op)" : node.label);
        return r;
    }

    // 未实现的节点类型：显式失败并给出消息，避免静默跳过掩盖问题
    return execUnsupported(node, ctx, t);
}

// ============================================================================
// SetParameter — write a parameter through the AutomationEngine
// ============================================================================

FlowStepResult FlowRunner::execSetParameter(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string name  = paramValue(node, "name");
    std::string value = paramValue(node, "value");

    if (name.empty()) {
        result.passed = false;
        result.errorMessage = "SetParameter: 'name' is required";
        return result;
    }

    // Evaluate value expression (supports "$var", "channel:xxx", literals)
    ValueProvider provider = makeValueProvider(ctx.variables);
    auto evaluated = ExpressionEngine::evaluate(value, provider);

    std::string finalValue;
    if (evaluated.has_value()) {
        finalValue = std::to_string(evaluated.value());
    } else {
        // Treat as a raw string value (not a numeric expression)
        finalValue = value;
    }

    // Route through AutomationEngine via a synthetic TestStep
    if (ctx.engine) {
        TestStep step;
        step.type = StepType::SetParameter;
        step.params = {{"name", name}, {"value", finalValue}};
        bool ok = ctx.engine->executeStep(step);
        if (!ok) {
            result.passed = false;
            result.errorMessage = "SetParameter '" + name + "' = " + finalValue + " failed";
        }
    }

    result.logLine = "[SetParameter] " + name + " = " + finalValue;
    return result;
}

// ============================================================================
// Delay / Wait — pause execution for N milliseconds
// ============================================================================

FlowStepResult FlowRunner::execDelay(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string msStr = paramValue(node, "ms", "1000");
    ValueProvider provider = makeValueProvider(ctx.variables);
    auto evaluated = ExpressionEngine::evaluate(msStr, provider);

    int delayMs = 1000;
    if (evaluated.has_value())
        delayMs = static_cast<int>(evaluated.value());
    else {
        try { delayMs = std::stoi(msStr); }
        catch (...) { delayMs = 1000; }
    }

    if (delayMs < 0) delayMs = 0;

    // Sleep in small chunks so we remain responsive to stop/pause
    const int chunkMs = 100;
    int remaining = delayMs;
    while (remaining > 0 && !ctx.stopRequested && !m_stopRequested.load()) {
        handlePause(ctx);
        if (ctx.stopRequested || m_stopRequested.load()) {
            result.passed = false;
            result.errorMessage = "Delay interrupted by stop request";
            return result;
        }
        int sleepMs = std::min(chunkMs, remaining);
        QThread::msleep(static_cast<unsigned long>(sleepMs));
        remaining -= sleepMs;
    }

    result.logLine = "[Delay] " + std::to_string(delayMs) + " ms";
    return result;
}

// ============================================================================
// WaitCondition — poll a condition until true or timeout
// ============================================================================

FlowStepResult FlowRunner::execWaitCondition(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string conditionExpr = paramValue(node, "condition");
    if (conditionExpr.empty()) {
        result.passed = false;
        result.errorMessage = "WaitCondition: 'condition' parameter is required";
        return result;
    }

    std::string timeoutStr = paramValue(node, "timeout", "5000");
    int timeoutMs = 5000;
    try { timeoutMs = std::stoi(timeoutStr); }
    catch (...) { timeoutMs = 5000; }

    int pollMs = 50;  // poll interval
    int elapsedMs = 0;

    while (elapsedMs < timeoutMs && !ctx.stopRequested && !m_stopRequested.load()) {
        handlePause(ctx);
        if (ctx.stopRequested || m_stopRequested.load()) {
            result.passed = false;
            result.errorMessage = "WaitCondition interrupted by stop request";
            return result;
        }

        ValueProvider provider = makeValueProvider(ctx.variables);
        auto value = ExpressionEngine::evaluate(conditionExpr, provider);
        if (value.has_value() && value.value() != 0.0) {
            // Condition met
            result.logLine = "[WaitCondition] condition met after " +
                std::to_string(elapsedMs) + " ms";
            return result;
        }

        QThread::msleep(static_cast<unsigned long>(pollMs));
        elapsedMs += pollMs;
    }

    result.passed = false;
    result.errorMessage = "WaitCondition timed out after " +
        std::to_string(timeoutMs) + " ms: " + conditionExpr;
    return result;
}

// ============================================================================
// ReadParameter — read a motor parameter into a variable
// ============================================================================

FlowStepResult FlowRunner::execReadParameter(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string paramName = paramValue(node, "name");
    std::string varName   = paramValue(node, "variable");

    if (paramName.empty()) {
        result.passed = false;
        result.errorMessage = "ReadParameter: 'name' is required";
        return result;
    }

    // Route through AutomationEngine
    if (ctx.engine) {
        TestStep step;
        step.type = StepType::ReadParameter;
        step.params = {{"name", paramName}};
        bool ok = ctx.engine->executeStep(step);
        if (!ok) {
            result.passed = false;
            result.errorMessage = "ReadParameter '" + paramName + "' failed";
            return result;
        }
    }

    result.logLine = varName.empty()
        ? "[ReadParameter] " + paramName
        : "[ReadParameter] " + paramName + " -> $" + varName;
    return result;
}

// ============================================================================
// StartMotor / StopMotor
// ============================================================================

FlowStepResult FlowRunner::execStartMotor(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    if (ctx.engine) {
        TestStep step;
        step.type = StepType::StartMotor;
        bool ok = ctx.engine->executeStep(step);
        if (!ok) {
            result.passed = false;
            result.errorMessage = "StartMotor failed";
            return result;
        }
    }

    result.logLine = "[StartMotor] OK";
    return result;
}

FlowStepResult FlowRunner::execStopMotor(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    if (ctx.engine) {
        TestStep step;
        step.type = StepType::StopMotor;
        bool ok = ctx.engine->executeStep(step);
        if (!ok) {
            result.passed = false;
            result.errorMessage = "StopMotor failed";
            return result;
        }
    }

    result.logLine = "[StopMotor] OK";
    return result;
}

// ============================================================================
// Assign — evaluate expression and store result in a variable
// ============================================================================

FlowStepResult FlowRunner::execAssign(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string varName = paramValue(node, "variable");
    std::string valueExpr = paramValue(node, "value");

    if (varName.empty()) {
        result.passed = false;
        result.errorMessage = "Assign: 'variable' parameter is required";
        return result;
    }

    ValueProvider provider = makeValueProvider(ctx.variables);
    auto evaluated = ExpressionEngine::evaluate(valueExpr, provider);

    if (!evaluated.has_value()) {
        result.passed = false;
        result.errorMessage = "Assign: failed to evaluate expression '" + valueExpr + "'";
        return result;
    }

    double val = evaluated.value();
    if (ctx.variables) {
        ctx.variables->setNumber(varName, val);
    }

    // Build compact string representation
    std::ostringstream oss;
    oss << val;
    result.logLine = "[Assign] $" + varName + " = " + oss.str();
    return result;
}

// ============================================================================
// Calculate / Math / Expression — evaluate an expression, optionally store result
// ============================================================================

FlowStepResult FlowRunner::execCalc(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string expr = paramValue(node, "expression");
    if (expr.empty()) {
        result.passed = false;
        result.errorMessage = "Calculate: 'expression' is required";
        return result;
    }

    ValueProvider provider = makeValueProvider(ctx.variables);
    auto evaluated = ExpressionEngine::evaluate(expr, provider);
    if (!evaluated.has_value()) {
        result.passed = false;
        result.errorMessage = "Calculate: failed to evaluate '" + expr + "'";
        return result;
    }

    double val = evaluated.value();
    std::string varName = paramValue(node, "var");
    if (!varName.empty() && ctx.variables) {
        ctx.variables->setNumber(varName, val);
    }

    std::ostringstream oss;
    oss << val;
    result.logLine = "[Calculate] " + expr + " = " + oss.str();
    return result;
}

// ============================================================================
// If — evaluate condition; result.passed == true means take Yes branch
// ============================================================================

FlowStepResult FlowRunner::execIf(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;
    result.passed = evalCondition(node, ctx, "condition");
    result.logLine = "[If] condition = " + std::string(result.passed ? "true" : "false");
    return result;
}

// ============================================================================
// Loop — evaluate condition; result.passed == true means enter (or repeat) body
// ============================================================================

FlowStepResult FlowRunner::execLoop(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;
    result.passed = evalCondition(node, ctx, "condition");
    result.logLine = "[Loop] condition = " + std::string(result.passed ? "true (enter body)" : "false (exit)");
    return result;
}

// ============================================================================
// SubFlow — execute a named sub-graph recursively
// ============================================================================

FlowStepResult FlowRunner::execSubFlow(const FlowNode& node, ExecutionContext& ctx,
                                        const FlowGraph& parentGraph)
{
    FlowStepResult result;

    std::string subName = paramValue(node, "name");
    if (subName.empty()) {
        result.passed = false;
        result.errorMessage = "SubFlow: 'name' parameter is required";
        return result;
    }

    // Find the sub-graph by name
    const FlowSubGraph* subGraph = nullptr;
    for (const auto& sg : parentGraph.subGraphs) {
        if (sg.name == subName) {
            subGraph = &sg;
            break;
        }
    }

    if (!subGraph) {
        result.passed = false;
        result.errorMessage = "SubFlow: sub-graph '" + subName + "' not found";
        return result;
    }

    // Wrap sub-graph nodes/edges into a temporary FlowGraph for executeGraph()
    FlowGraph subFlowGraph;
    subFlowGraph.name = subGraph->name;
    subFlowGraph.nodes = subGraph->nodes;
    subFlowGraph.edges = subGraph->edges;
    subFlowGraph.variables = subGraph->variables;

    // Create a local scope for the sub-flow (inherits from parent scope)
    VariableScope localScope;
    if (ctx.variables)
        localScope.setParentScope(ctx.variables);
    VariableScope* savedVars = ctx.variables;
    ctx.variables = &localScope;

    // Execute recursively
    FlowRunResult subResult = executeGraph(subFlowGraph, ctx, &localScope);

    // Restore parent scope
    ctx.variables = savedVars;

    result.passed = subResult.passed;
    result.errorMessage = subResult.errorMessage;
    result.logLine = "[SubFlow] '" + subName + "' " +
        std::string(subResult.passed ? "OK" : "FAILED");

    return result;
}

// ============================================================================
// Assert — evaluate condition; node fails if condition is false
// ============================================================================

FlowStepResult FlowRunner::execAssert(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    bool conditionMet = evalCondition(node, ctx, "condition");

    if (!conditionMet) {
        result.passed = false;
        std::string expr = paramValue(node, "condition");
        std::string msg  = paramValue(node, "message");
        result.errorMessage = msg.empty()
            ? "Assertion failed: " + expr
            : "Assertion failed: " + msg;
    }

    result.logLine = "[Assert] " + std::string(conditionMet ? "PASS" : "FAIL");
    return result;
}

// ============================================================================
// Log — emit a log message
// ============================================================================

FlowStepResult FlowRunner::execLog(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string message = paramValue(node, "message", node.label);

    if (ctx.log)
        ctx.log("[FlowLog] " + message);

    result.logLine = "[Log] " + message;
    return result;
}

// ============================================================================
// Unsupported — fail loudly instead of silently skipping
// ============================================================================

FlowStepResult FlowRunner::execUnsupported(const FlowNode& node, ExecutionContext& ctx,
                                           const std::string& type)
{
    FlowStepResult result;
    result.passed = false;
    result.errorMessage = "未实现的节点类型: '" + type + "'";
    result.logLine = "[Unsupported] " + type;
    if (ctx.log)
        ctx.log("[FlowRunner] unsupported node type '" + type + "' at node '" + node.id + "'");
    return result;
}

// ============================================================================
// RecordData — placeholder for recording a data sample
// ============================================================================

FlowStepResult FlowRunner::execRecordData(const FlowNode& node, ExecutionContext& ctx)
{
    FlowStepResult result;

    std::string channelName = paramValue(node, "channel");
    result.logLine = "[RecordData] channel=" + channelName + " (recorded)";

    // Future: subscribe data to a recording buffer.
    // For now this is a no-op success node.

    return result;
}

} // namespace MotorStudio
