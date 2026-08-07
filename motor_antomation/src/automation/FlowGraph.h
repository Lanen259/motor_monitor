#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace MotorStudio {

// Forward declaration for fromTestCase conversion
struct TestCase;
struct TestStep;
enum class StepType : uint8_t;

// ============================================================
// PortType — which output port an edge originates from
// ============================================================
enum class PortType { Default, Yes, No };

inline const char* portTypeToString(PortType pt) {
    switch (pt) {
        case PortType::Yes:  return "yes";
        case PortType::No:   return "no";
        default:             return "default";
    }
}

inline PortType portTypeFromString(const std::string& s) {
    if (s == "yes")  return PortType::Yes;
    if (s == "no")   return PortType::No;
    return PortType::Default;
}

// ============================================================
// FlowNode — a single node in the flow graph
// ============================================================
struct FlowNode {
    std::string id;                                    // unique identifier
    std::string type;                                  // node type string (SetParameter, Delay, If, ...)
    std::string label;                                 // display name
    std::vector<std::pair<std::string, std::string>> params; // key-value parameters
    double posX = 0.0;                                 // canvas X position
    double posY = 0.0;                                 // canvas Y position
};

// ============================================================
// FlowEdge — a directed edge connecting two nodes
// ============================================================
struct FlowEdge {
    std::string id;              // unique identifier
    std::string fromNodeId;      // source node id
    std::string toNodeId;        // target node id
    PortType fromPort = PortType::Default;  // which output port on the source node
};

// ============================================================
// FlowSubGraph — a named, reusable sub-flowchart
// ============================================================
struct FlowSubGraph {
    std::string name;
    std::vector<FlowNode> nodes;
    std::vector<FlowEdge> edges;
    std::vector<std::string> variables;  // variable declarations scoped to this sub-graph
};

// ============================================================
// FlowGraph — the complete flow graph data model
// ============================================================
struct FlowGraph {
    std::string name;
    std::string description;
    std::vector<FlowNode> nodes;
    std::vector<FlowEdge> edges;
    std::vector<std::string> variables;      // global variable declarations
    std::vector<FlowSubGraph> subGraphs;     // reusable sub-flows

    // JSON serialization
    QJsonObject toJson() const;
    static std::optional<FlowGraph> fromJson(const QJsonObject& json);

    // Compatibility: convert old table-based TestCase to FlowGraph
    static FlowGraph fromTestCase(const TestCase& tc);

    // Graph queries
    const FlowNode* findNode(const std::string& id) const;
    std::vector<const FlowEdge*> edgesFrom(const std::string& nodeId) const;
    const FlowNode* startNode() const;  // node with no incoming edges
};

} // namespace MotorStudio
