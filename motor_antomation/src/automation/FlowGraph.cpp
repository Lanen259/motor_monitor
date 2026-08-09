#include "FlowGraph.h"
#include "AutomationEngine.h"  // for TestCase, TestStep, StepType

#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <algorithm>
#include <sstream>

namespace MotorStudio {

// ============================================================
// Internal helpers
// ============================================================

namespace {

// Generate sequential IDs: "n1", "n2", "e1", "e2", etc.
struct IdGenerator {
    int counter = 0;
    std::string next(const char* prefix) {
        std::ostringstream oss;
        oss << prefix << (++counter);
        return oss.str();
    }
    void reset() { counter = 0; }
};

// Map StepType enum to FlowNode type string
std::string stepTypeToFlowNodeType(StepType st) {
    switch (st) {
        case StepType::SetParameter:  return "SetParameter";
        case StepType::Wait:          return "Delay";
        case StepType::ReadParameter: return "ReadParameter";
        case StepType::Assert:        return "Assert";
        case StepType::RecordData:    return "RecordData";
        case StepType::StartMotor:    return "StartMotor";
        case StepType::StopMotor:     return "StopMotor";
        case StepType::Custom:        return "CustomCommand";
        default:                      return "Comment";
    }
}

} // anonymous namespace

// ============================================================
// JSON serialization
// ============================================================

QJsonObject FlowGraph::toJson() const
{
    QJsonObject root;
    root["name"]        = QString::fromStdString(name);
    root["description"] = QString::fromStdString(description);

    // Variables — array of strings
    QJsonArray varArray;
    for (const auto& v : variables) {
        varArray.append(QString::fromStdString(v));
    }
    root["variables"] = varArray;

    // Nodes
    QJsonArray nodeArray;
    for (const auto& node : nodes) {
        QJsonObject nodeObj;
        nodeObj["id"]    = QString::fromStdString(node.id);
        nodeObj["type"]  = QString::fromStdString(node.type);
        nodeObj["label"] = QString::fromStdString(node.label);
        nodeObj["posX"]  = node.posX;
        nodeObj["posY"]  = node.posY;

        // Params as JSON object (key-value map) — matches existing codebase convention
        QJsonObject paramsObj;
        for (const auto& kv : node.params) {
            paramsObj[QString::fromStdString(kv.first)] = QString::fromStdString(kv.second);
        }
        nodeObj["params"] = paramsObj;

        nodeArray.append(nodeObj);
    }
    root["nodes"] = nodeArray;

    // Edges
    QJsonArray edgeArray;
    for (const auto& edge : edges) {
        QJsonObject edgeObj;
        edgeObj["id"]         = QString::fromStdString(edge.id);
        edgeObj["fromNodeId"] = QString::fromStdString(edge.fromNodeId);
        edgeObj["toNodeId"]   = QString::fromStdString(edge.toNodeId);
        edgeObj["fromPort"]   = QString::fromLatin1(portTypeToString(edge.fromPort));
        edgeArray.append(edgeObj);
    }
    root["edges"] = edgeArray;

    // SubGraphs
    QJsonArray subArray;
    for (const auto& sg : subGraphs) {
        QJsonObject sgObj;
        sgObj["name"] = QString::fromStdString(sg.name);

        // Variables (scoped)
        QJsonArray sgVarArr;
        for (const auto& v : sg.variables) {
            sgVarArr.append(QString::fromStdString(v));
        }
        sgObj["variables"] = sgVarArr;

        // Nodes inside sub-graph (recursive structure, though not self-referencing)
        QJsonArray sgNodes;
        for (const auto& n : sg.nodes) {
            QJsonObject nObj;
            nObj["id"]    = QString::fromStdString(n.id);
            nObj["type"]  = QString::fromStdString(n.type);
            nObj["label"] = QString::fromStdString(n.label);
            nObj["posX"]  = n.posX;
            nObj["posY"]  = n.posY;
            QJsonObject pObj;
            for (const auto& kv : n.params) {
                pObj[QString::fromStdString(kv.first)] = QString::fromStdString(kv.second);
            }
            nObj["params"] = pObj;
            sgNodes.append(nObj);
        }
        sgObj["nodes"] = sgNodes;

        // Edges inside sub-graph
        QJsonArray sgEdges;
        for (const auto& e : sg.edges) {
            QJsonObject eObj;
            eObj["id"]         = QString::fromStdString(e.id);
            eObj["fromNodeId"] = QString::fromStdString(e.fromNodeId);
            eObj["toNodeId"]   = QString::fromStdString(e.toNodeId);
            eObj["fromPort"]   = QString::fromLatin1(portTypeToString(e.fromPort));
            sgEdges.append(eObj);
        }
        sgObj["edges"] = sgEdges;

        subArray.append(sgObj);
    }
    root["subGraphs"] = subArray;

    return root;
}

// ============================================================
// JSON deserialization
// ============================================================

std::optional<FlowGraph> FlowGraph::fromJson(const QJsonObject& json)
{
    FlowGraph graph;

    // Required: "name" (string)
    if (!json.contains("name") || !json["name"].isString()) {
        qWarning() << "FlowGraph::fromJson: missing or invalid 'name' field";
        return std::nullopt;
    }
    graph.name = json["name"].toString().toStdString();

    // Optional: "description"
    graph.description = json.value("description").toString("").toStdString();

    // Optional: "variables"
    if (json.contains("variables") && json["variables"].isArray()) {
        QJsonArray varArr = json["variables"].toArray();
        for (const auto& v : varArr) {
            graph.variables.push_back(v.toString().toStdString());
        }
    }

    // Required: "nodes" (array)
    if (!json.contains("nodes") || !json["nodes"].isArray()) {
        qWarning() << "FlowGraph::fromJson: missing or invalid 'nodes' array";
        return std::nullopt;
    }
    QJsonArray nodeArr = json["nodes"].toArray();
    for (const auto& nv : nodeArr) {
        if (!nv.isObject()) continue;
        QJsonObject nObj = nv.toObject();

        FlowNode node;
        node.id    = nObj.value("id").toString("").toStdString();
        node.type  = nObj.value("type").toString("Comment").toStdString();
        node.label = nObj.value("label").toString("").toStdString();
        node.posX  = nObj.value("posX").toDouble(0.0);
        node.posY  = nObj.value("posY").toDouble(0.0);

        // Params as JSON object
        if (nObj.contains("params") && nObj["params"].isObject()) {
            QJsonObject pObj = nObj["params"].toObject();
            for (auto it = pObj.begin(); it != pObj.end(); ++it) {
                std::string key = it.key().toStdString();
                std::string value;
                if (it.value().isString()) {
                    value = it.value().toString().toStdString();
                } else if (it.value().isDouble()) {
                    value = std::to_string(it.value().toDouble());
                } else {
                    value = it.value().toString().toStdString();
                }
                node.params.emplace_back(key, value);
            }
        }

        // Validate: id must not be empty
        if (node.id.empty()) {
            qWarning() << "FlowGraph::fromJson: node with empty id, skipping";
            continue;
        }

        graph.nodes.push_back(std::move(node));
    }

    // Optional: "edges" (array)
    if (json.contains("edges") && json["edges"].isArray()) {
        QJsonArray edgeArr = json["edges"].toArray();
        for (const auto& ev : edgeArr) {
            if (!ev.isObject()) continue;
            QJsonObject eObj = ev.toObject();

            FlowEdge edge;
            edge.id         = eObj.value("id").toString("").toStdString();
            edge.fromNodeId = eObj.value("fromNodeId").toString("").toStdString();
            edge.toNodeId   = eObj.value("toNodeId").toString("").toStdString();
            edge.fromPort   = portTypeFromString(
                eObj.value("fromPort").toString("default").toStdString());

            if (edge.id.empty() || edge.fromNodeId.empty() || edge.toNodeId.empty()) {
                qWarning() << "FlowGraph::fromJson: edge with missing fields, skipping";
                continue;
            }

            graph.edges.push_back(std::move(edge));
        }
    }

    // Optional: "subGraphs" (array)
    if (json.contains("subGraphs") && json["subGraphs"].isArray()) {
        QJsonArray subArr = json["subGraphs"].toArray();
        for (const auto& sv : subArr) {
            if (!sv.isObject()) continue;
            QJsonObject sgObj = sv.toObject();

            FlowSubGraph sg;
            sg.name = sgObj.value("name").toString("").toStdString();

            // Sub-graph variables
            if (sgObj.contains("variables") && sgObj["variables"].isArray()) {
                QJsonArray vArr = sgObj["variables"].toArray();
                for (const auto& v : vArr) {
                    sg.variables.push_back(v.toString().toStdString());
                }
            }

            // Sub-graph nodes
            if (sgObj.contains("nodes") && sgObj["nodes"].isArray()) {
                QJsonArray sgNodeArr = sgObj["nodes"].toArray();
                for (const auto& nv : sgNodeArr) {
                    if (!nv.isObject()) continue;
                    QJsonObject nObj = nv.toObject();
                    FlowNode node;
                    node.id    = nObj.value("id").toString("").toStdString();
                    node.type  = nObj.value("type").toString("Comment").toStdString();
                    node.label = nObj.value("label").toString("").toStdString();
                    node.posX  = nObj.value("posX").toDouble(0.0);
                    node.posY  = nObj.value("posY").toDouble(0.0);

                    if (nObj.contains("params") && nObj["params"].isObject()) {
                        QJsonObject pObj = nObj["params"].toObject();
                        for (auto it = pObj.begin(); it != pObj.end(); ++it) {
                            std::string value;
                            if (it.value().isString())
                                value = it.value().toString().toStdString();
                            else if (it.value().isDouble())
                                value = std::to_string(it.value().toDouble());
                            else
                                value = it.value().toString().toStdString();
                            node.params.emplace_back(it.key().toStdString(), value);
                        }
                    }

                    if (!node.id.empty()) {
                        sg.nodes.push_back(std::move(node));
                    }
                }
            }

            // Sub-graph edges
            if (sgObj.contains("edges") && sgObj["edges"].isArray()) {
                QJsonArray sgEdgeArr = sgObj["edges"].toArray();
                for (const auto& ev : sgEdgeArr) {
                    if (!ev.isObject()) continue;
                    QJsonObject eObj = ev.toObject();
                    FlowEdge edge;
                    edge.id         = eObj.value("id").toString("").toStdString();
                    edge.fromNodeId = eObj.value("fromNodeId").toString("").toStdString();
                    edge.toNodeId   = eObj.value("toNodeId").toString("").toStdString();
                    edge.fromPort   = portTypeFromString(
                        eObj.value("fromPort").toString("default").toStdString());

                    if (!edge.id.empty() && !edge.fromNodeId.empty() && !edge.toNodeId.empty()) {
                        sg.edges.push_back(std::move(edge));
                    }
                }
            }

            graph.subGraphs.push_back(std::move(sg));
        }
    }

    return graph;
}

// ============================================================
// Compatibility: convert old table-based TestCase to FlowGraph
// ============================================================

FlowGraph FlowGraph::fromTestCase(const TestCase& tc)
{
    FlowGraph graph;
    graph.name        = tc.name;
    graph.description = tc.description;

    // Store stopOnFailure as a variable so it can be inspected
    if (tc.stopOnFailure) {
        graph.variables.push_back("__stopOnFailure: true");
    } else {
        graph.variables.push_back("__stopOnFailure: false");
    }

    IdGenerator idGen;

    // Helper: find a param value in a TestStep
    auto stepParam = [](const TestStep& s, const std::string& key) -> std::string {
        for (const auto& kv : s.params) {
            if (kv.first == key) return kv.second;
        }
        return "";
    };

    // Convert each TestStep → FlowNode(s)。
    // 旧表格的参数键与 FlowRunner 执行器要求的键不一致，这里做一次翻译：
    //  - SetParameter: {Speed:"1500"} → {name:"Speed", value:"1500"}（多个参数拆成多个节点）
    //  - Wait:         {durationMs:"2000"} → {ms:"2000"}
    //  - Assert:       {channel,min,max} → {condition:"channel:xx >= min && channel:xx <= max"}
    for (const auto& step : tc.steps) {
        const std::string nodeType = stepTypeToFlowNodeType(step.type);

        if (nodeType == "SetParameter") {
            bool hasNameValue = stepParam(step, "name") != "" || stepParam(step, "value") != "";
            if (!hasNameValue) {
                // 每个键值对 → 一个 SetParameter 节点
                for (const auto& kv : step.params) {
                    FlowNode node;
                    node.id    = idGen.next("n");
                    node.type  = nodeType;
                    node.label = step.description;
                    node.params = {{"name", kv.first}, {"value", kv.second}};
                    node.posX = 100.0;
                    node.posY = 100.0 + static_cast<double>(graph.nodes.size()) * 100.0;
                    graph.nodes.push_back(std::move(node));
                }
            } else {
                FlowNode node;
                node.id    = idGen.next("n");
                node.type  = nodeType;
                node.label = step.description;
                node.params = step.params;
                node.posX = 100.0;
                node.posY = 100.0 + static_cast<double>(graph.nodes.size()) * 100.0;
                graph.nodes.push_back(std::move(node));
            }
        } else if (nodeType == "Delay") {
            FlowNode node;
            node.id    = idGen.next("n");
            node.type  = nodeType;
            node.label = step.description;
            // durationMs → ms
            std::string dur = stepParam(step, "durationMs");
            node.params = dur.empty() ? step.params
                                      : std::vector<std::pair<std::string, std::string>>{{"ms", dur}};
            node.posX = 100.0;
            node.posY = 100.0 + static_cast<double>(graph.nodes.size()) * 100.0;
            graph.nodes.push_back(std::move(node));
        } else if (nodeType == "Assert") {
            // channel/min/max → 条件表达式（channel:xx 由表达式引擎解析）
            std::string channel = stepParam(step, "channel");
            std::string minStr  = stepParam(step, "min");
            std::string maxStr  = stepParam(step, "max");

            std::string cond;
            if (!channel.empty()) {
                std::string ref = "channel:" + channel;
                if (!minStr.empty()) cond = ref + " >= " + minStr;
                if (!maxStr.empty()) {
                    if (!cond.empty()) cond += " && ";
                    cond += ref + " <= " + maxStr;
                }
            } else {
                cond = stepParam(step, "expression");
            }

            FlowNode node;
            node.id    = idGen.next("n");
            node.type  = nodeType;
            node.label = step.description;
            node.params = {{"condition", cond}, {"message", step.description}};
            node.posX = 100.0;
            node.posY = 100.0 + static_cast<double>(graph.nodes.size()) * 100.0;
            graph.nodes.push_back(std::move(node));
        } else {
            FlowNode node;
            node.id    = idGen.next("n");
            node.type  = nodeType;
            node.label = step.description;
            node.params = step.params;
            node.posX = 100.0;
            node.posY = 100.0 + static_cast<double>(graph.nodes.size()) * 100.0;
            graph.nodes.push_back(std::move(node));
        }

        // Also store timeoutMs and retryCount as params if non-default
        if (step.timeoutMs != 5000) {
            graph.nodes.back().params.emplace_back("__timeoutMs", std::to_string(step.timeoutMs));
        }
        if (step.retryCount != 0) {
            graph.nodes.back().params.emplace_back("__retryCount", std::to_string(step.retryCount));
        }
    }

    // Connect sequentially: node[i] → node[i+1]
    if (graph.nodes.size() > 1) {
        for (size_t i = 1; i < graph.nodes.size(); ++i) {
            FlowEdge edge;
            edge.id         = idGen.next("e");
            edge.fromNodeId = graph.nodes[i - 1].id;
            edge.toNodeId   = graph.nodes[i].id;
            graph.edges.push_back(std::move(edge));
        }
    }

    return graph;
}

// ============================================================
// Graph queries
// ============================================================

const FlowNode* FlowGraph::findNode(const std::string& id) const
{
    for (const auto& node : nodes) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

std::vector<const FlowEdge*> FlowGraph::edgesFrom(const std::string& nodeId) const
{
    std::vector<const FlowEdge*> result;
    for (const auto& edge : edges) {
        if (edge.fromNodeId == nodeId) {
            result.push_back(&edge);
        }
    }
    return result;
}

const FlowNode* FlowGraph::startNode() const
{
    if (nodes.empty()) return nullptr;

    // Collect all node ids that have incoming edges
    std::vector<std::string> hasIncoming;
    for (const auto& edge : edges) {
        hasIncoming.push_back(edge.toNodeId);
    }

    // Find the first node whose id is NOT in hasIncoming
    for (const auto& node : nodes) {
        bool incoming = false;
        for (const auto& target : hasIncoming) {
            if (node.id == target) {
                incoming = true;
                break;
            }
        }
        if (!incoming) {
            return &node;
        }
    }

    // All nodes have incoming edges (cycle) — return first node
    return &nodes[0];
}

} // namespace MotorStudio
