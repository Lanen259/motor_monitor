#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "../../src/automation/FlowGraph.h"
#include "../../src/automation/AutomationEngine.h"

using namespace MotorStudio;

// ============================================================================
// TestFlowGraph — unit tests for FlowGraph data model
// ============================================================================

class TestFlowGraph : public QObject {
    Q_OBJECT

private:
    QString toJsonString(const FlowGraph& graph) const {
        QJsonDocument doc(graph.toJson());
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }

    FlowGraph buildSimpleGraph() const {
        FlowGraph g;
        g.name        = "SimpleTest";
        g.description = "A simple two-node graph";
        g.variables   = {"speed", "temp"};

        FlowNode n1;
        n1.id    = "n1";
        n1.type  = "StartMotor";
        n1.label = "启动电机";
        n1.posX  = 100.0;
        n1.posY  = 100.0;
        g.nodes.push_back(n1);

        FlowNode n2;
        n2.id    = "n2";
        n2.type  = "Delay";
        n2.label = "等待稳定";
        n2.posX  = 300.0;
        n2.posY  = 100.0;
        n2.params = {{"durationMs", "2000"}};
        g.nodes.push_back(n2);

        FlowEdge e1;
        e1.id         = "e1";
        e1.fromNodeId = "n1";
        e1.toNodeId   = "n2";
        e1.fromPort   = PortType::Default;
        g.edges.push_back(e1);

        return g;
    }

    FlowGraph buildGraphWithSubGraph() const {
        FlowGraph g;
        g.name        = "WithSubGraph";
        g.description = "Graph with a sub-flow";

        FlowNode n1;
        n1.id = "n1"; n1.type = "StartMotor"; n1.label = "启动";
        g.nodes.push_back(n1);

        FlowNode n2;
        n2.id = "n2"; n2.type = "SubFlow"; n2.label = "调用校准流程";
        n2.params = {{"subGraph", "CalibrationRoutine"}};
        g.nodes.push_back(n2);

        FlowNode n3;
        n3.id = "n3"; n3.type = "StopMotor"; n3.label = "停止";
        g.nodes.push_back(n3);

        FlowEdge e1;
        e1.id = "e1"; e1.fromNodeId = "n1"; e1.toNodeId = "n2";
        g.edges.push_back(e1);

        FlowEdge e2;
        e2.id = "e2"; e2.fromNodeId = "n2"; e2.toNodeId = "n3";
        g.edges.push_back(e2);

        FlowSubGraph sg;
        sg.name = "CalibrationRoutine";
        sg.variables = {"factor"};

        FlowNode sn1;
        sn1.id = "s1"; sn1.type = "SetParameter"; sn1.label = "设置增益";
        sn1.params = {{"Gain", "1.5"}};
        sg.nodes.push_back(sn1);

        FlowNode sn2;
        sn2.id = "s2"; sn2.type = "Delay"; sn2.label = "等待";
        sn2.params = {{"durationMs", "500"}};
        sg.nodes.push_back(sn2);

        FlowEdge se1;
        se1.id = "se1"; se1.fromNodeId = "s1"; se1.toNodeId = "s2";
        sg.edges.push_back(se1);

        g.subGraphs.push_back(sg);
        return g;
    }

    TestCase buildTestCase() const {
        TestCase tc;
        tc.name        = "BasicMotorTest";
        tc.description = "Motor start/stop sequence";
        tc.stopOnFailure = true;

        TestStep s1;
        s1.type = StepType::StartMotor;
        s1.description = "启动电机";
        tc.steps.push_back(s1);

        TestStep s2;
        s2.type = StepType::Wait;
        s2.description = "等待2秒";
        s2.params = {{"durationMs", "2000"}};
        s2.timeoutMs = 5000;
        tc.steps.push_back(s2);

        TestStep s3;
        s3.type = StepType::SetParameter;
        s3.description = "设置转速1000RPM";
        s3.params = {{"Speed", "1000"}};
        tc.steps.push_back(s3);

        TestStep s4;
        s4.type = StepType::Assert;
        s4.description = "验证转速范围";
        s4.params = {{"channel", "Speed"}, {"min", "900"}, {"max", "1100"}};
        tc.steps.push_back(s4);

        TestStep s5;
        s5.type = StepType::StopMotor;
        s5.description = "停止电机";
        tc.steps.push_back(s5);

        return tc;
    }

private slots:
    // --------------------------------------------------------------------
    // Serialization: round-trip (build → JSON → parse → compare)
    // --------------------------------------------------------------------
    void testRoundTripSimple()
    {
        FlowGraph original = buildSimpleGraph();

        // Serialize
        QJsonObject json = original.toJson();
        QString jsonStr = toJsonString(original);
        QVERIFY(!jsonStr.isEmpty());

        // Deserialize
        auto parsed = FlowGraph::fromJson(json);
        QVERIFY(parsed.has_value());

        const FlowGraph& g = *parsed;
        QCOMPARE(QString::fromStdString(g.name), QString::fromStdString(original.name));
        QCOMPARE(QString::fromStdString(g.description), QString::fromStdString(original.description));

        // Variables
        QCOMPARE(g.variables.size(), original.variables.size());
        for (size_t i = 0; i < g.variables.size(); ++i) {
            QCOMPARE(QString::fromStdString(g.variables[i]),
                     QString::fromStdString(original.variables[i]));
        }

        // Nodes
        QCOMPARE(g.nodes.size(), original.nodes.size());
        for (size_t i = 0; i < g.nodes.size(); ++i) {
            QCOMPARE(QString::fromStdString(g.nodes[i].id),
                     QString::fromStdString(original.nodes[i].id));
            QCOMPARE(QString::fromStdString(g.nodes[i].type),
                     QString::fromStdString(original.nodes[i].type));
            QCOMPARE(QString::fromStdString(g.nodes[i].label),
                     QString::fromStdString(original.nodes[i].label));
            QCOMPARE(g.nodes[i].posX, original.nodes[i].posX);
            QCOMPARE(g.nodes[i].posY, original.nodes[i].posY);
            QCOMPARE(g.nodes[i].params.size(), original.nodes[i].params.size());
        }

        // Edges
        QCOMPARE(g.edges.size(), original.edges.size());
        for (size_t i = 0; i < g.edges.size(); ++i) {
            QCOMPARE(QString::fromStdString(g.edges[i].id),
                     QString::fromStdString(original.edges[i].id));
            QCOMPARE(QString::fromStdString(g.edges[i].fromNodeId),
                     QString::fromStdString(original.edges[i].fromNodeId));
            QCOMPARE(QString::fromStdString(g.edges[i].toNodeId),
                     QString::fromStdString(original.edges[i].toNodeId));
            QCOMPARE(g.edges[i].fromPort, original.edges[i].fromPort);
        }
    }

    // --------------------------------------------------------------------
    // Serialization: verify the exact JSON structure
    // --------------------------------------------------------------------
    void testJsonStructure()
    {
        FlowGraph g = buildSimpleGraph();
        QJsonObject json = g.toJson();

        QVERIFY(json.contains("name"));
        QCOMPARE(json["name"].toString(), QString("SimpleTest"));

        QVERIFY(json.contains("description"));
        QCOMPARE(json["description"].toString(), QString("A simple two-node graph"));

        QVERIFY(json.contains("variables"));
        QJsonArray vars = json["variables"].toArray();
        QCOMPARE(vars.size(), 2);
        QCOMPARE(vars[0].toString(), QString("speed"));
        QCOMPARE(vars[1].toString(), QString("temp"));

        QVERIFY(json.contains("nodes"));
        QJsonArray nodeArr = json["nodes"].toArray();
        QCOMPARE(nodeArr.size(), 2);

        QJsonObject n0 = nodeArr[0].toObject();
        QCOMPARE(n0["id"].toString(), QString("n1"));
        QCOMPARE(n0["type"].toString(), QString("StartMotor"));
        QCOMPARE(n0["label"].toString(), QString("启动电机"));

        QJsonObject n1 = nodeArr[1].toObject();
        QCOMPARE(n1["id"].toString(), QString("n2"));
        QCOMPARE(n1["type"].toString(), QString("Delay"));
        QJsonObject n1params = n1["params"].toObject();
        QCOMPARE(n1params["durationMs"].toString(), QString("2000"));

        QVERIFY(json.contains("edges"));
        QJsonArray edgeArr = json["edges"].toArray();
        QCOMPARE(edgeArr.size(), 1);

        QJsonObject e0 = edgeArr[0].toObject();
        QCOMPARE(e0["fromNodeId"].toString(), QString("n1"));
        QCOMPARE(e0["toNodeId"].toString(), QString("n2"));

        QVERIFY(json.contains("subGraphs"));
        QJsonArray subArr = json["subGraphs"].toArray();
        QCOMPARE(subArr.size(), 0);
    }

    // --------------------------------------------------------------------
    // Deserialization: from JSON string
    // --------------------------------------------------------------------
    void testDeserializeFromString()
    {
        QString raw = R"({
            "name": "MotorCalibration",
            "description": "Motor calibration test",
            "variables": ["speed", "temp"],
            "nodes": [
                {
                    "id": "n1",
                    "type": "StartMotor",
                    "label": "启动电机",
                    "posX": 100,
                    "posY": 100,
                    "params": {}
                },
                {
                    "id": "n2",
                    "type": "Delay",
                    "label": "等待稳定",
                    "posX": 300,
                    "posY": 100,
                    "params": {"durationMs": "2000"}
                }
            ],
            "edges": [
                {
                    "id": "e1",
                    "fromNodeId": "n1",
                    "toNodeId": "n2",
                    "fromPort": "default"
                }
            ],
            "subGraphs": []
        })";

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
        QCOMPARE(err.error, QJsonParseError::NoError);

        auto optGraph = FlowGraph::fromJson(doc.object());
        QVERIFY(optGraph.has_value());

        const FlowGraph& g = *optGraph;
        QCOMPARE(QString::fromStdString(g.name), QString("MotorCalibration"));
        QCOMPARE(QString::fromStdString(g.description), QString("Motor calibration test"));
        QCOMPARE(static_cast<int>(g.variables.size()), 2);
        QCOMPARE(QString::fromStdString(g.variables[0]), QString("speed"));
        QCOMPARE(QString::fromStdString(g.variables[1]), QString("temp"));

        QCOMPARE(static_cast<int>(g.nodes.size()), 2);
        QCOMPARE(QString::fromStdString(g.nodes[0].id), QString("n1"));
        QCOMPARE(QString::fromStdString(g.nodes[0].type), QString("StartMotor"));
        QCOMPARE(g.nodes[0].posX, 100.0);
        QCOMPARE(g.nodes[0].posY, 100.0);

        QCOMPARE(QString::fromStdString(g.nodes[1].id), QString("n2"));
        QCOMPARE(QString::fromStdString(g.nodes[1].type), QString("Delay"));
        QCOMPARE(static_cast<int>(g.nodes[1].params.size()), 1);
        QCOMPARE(QString::fromStdString(g.nodes[1].params[0].first), QString("durationMs"));
        QCOMPARE(QString::fromStdString(g.nodes[1].params[0].second), QString("2000"));

        QCOMPARE(static_cast<int>(g.edges.size()), 1);
        QCOMPARE(QString::fromStdString(g.edges[0].fromNodeId), QString("n1"));
        QCOMPARE(QString::fromStdString(g.edges[0].toNodeId), QString("n2"));

        QCOMPARE(static_cast<int>(g.subGraphs.size()), 0);
    }

    // --------------------------------------------------------------------
    // Deserialization: invalid JSON -> nullopt
    // --------------------------------------------------------------------
    void testDeserializeInvalidJson()
    {
        // Missing "name" field
        {
            QJsonObject obj;
            obj["nodes"] = QJsonArray();
            auto opt = FlowGraph::fromJson(obj);
            QVERIFY(!opt.has_value());
        }

        // Missing "nodes" field
        {
            QJsonObject obj;
            obj["name"] = QString("Test");
            auto opt = FlowGraph::fromJson(obj);
            QVERIFY(!opt.has_value());
        }

        // "nodes" is not an array
        {
            QJsonObject obj;
            obj["name"] = QString("Test");
            obj["nodes"] = QString("not-array");
            auto opt = FlowGraph::fromJson(obj);
            QVERIFY(!opt.has_value());
        }

        // Empty object
        {
            QJsonObject obj;
            auto opt = FlowGraph::fromJson(obj);
            QVERIFY(!opt.has_value());
        }

        // Node with empty id — should be skipped, but graph should still parse
        // (at least one valid node required but that's not enforced)
        {
            QJsonObject obj;
            obj["name"] = QString("Test");
            QJsonArray nodeArr;
            QJsonObject badNode;
            badNode["id"] = QString("");  // empty id, will be skipped
            nodeArr.append(badNode);
            obj["nodes"] = nodeArr;
            auto opt = FlowGraph::fromJson(obj);
            // Graph parses successfully, but the bad node is skipped
            QVERIFY(opt.has_value());
            QCOMPARE(static_cast<int>(opt->nodes.size()), 0);
        }
    }

    // --------------------------------------------------------------------
    // Sub-graph serialization round-trip
    // --------------------------------------------------------------------
    void testSubGraphRoundTrip()
    {
        FlowGraph original = buildGraphWithSubGraph();
        QJsonObject json = original.toJson();

        auto parsed = FlowGraph::fromJson(json);
        QVERIFY(parsed.has_value());

        const FlowGraph& g = *parsed;
        QCOMPARE(QString::fromStdString(g.name), QString::fromStdString(original.name));
        QCOMPARE(static_cast<int>(g.nodes.size()), 3);
        QCOMPARE(static_cast<int>(g.edges.size()), 2);
        QCOMPARE(static_cast<int>(g.subGraphs.size()), 1);

        const FlowSubGraph& sg = g.subGraphs[0];
        QCOMPARE(QString::fromStdString(sg.name), QString("CalibrationRoutine"));
        QCOMPARE(static_cast<int>(sg.variables.size()), 1);
        QCOMPARE(QString::fromStdString(sg.variables[0]), QString("factor"));
        QCOMPARE(static_cast<int>(sg.nodes.size()), 2);
        QCOMPARE(static_cast<int>(sg.edges.size()), 1);

        // Verify sub-graph node details
        QCOMPARE(QString::fromStdString(sg.nodes[0].id), QString("s1"));
        QCOMPARE(QString::fromStdString(sg.nodes[0].type), QString("SetParameter"));
        QCOMPARE(static_cast<int>(sg.nodes[0].params.size()), 1);
        QCOMPARE(QString::fromStdString(sg.nodes[0].params[0].first), QString("Gain"));

        // Verify sub-graph edge
        QCOMPARE(QString::fromStdString(sg.edges[0].fromNodeId), QString("s1"));
        QCOMPARE(QString::fromStdString(sg.edges[0].toNodeId), QString("s2"));
    }

    // --------------------------------------------------------------------
    // Sub-graph JSON structure verification
    // --------------------------------------------------------------------
    void testSubGraphJsonStructure()
    {
        FlowGraph g = buildGraphWithSubGraph();
        QJsonObject json = g.toJson();

        QVERIFY(json.contains("subGraphs"));
        QJsonArray subArr = json["subGraphs"].toArray();
        QCOMPARE(subArr.size(), 1);

        QJsonObject sg = subArr[0].toObject();
        QCOMPARE(sg["name"].toString(), QString("CalibrationRoutine"));

        QVERIFY(sg.contains("variables"));
        QVERIFY(sg.contains("nodes"));
        QVERIFY(sg.contains("edges"));

        QJsonArray sgNodes = sg["nodes"].toArray();
        QCOMPARE(sgNodes.size(), 2);

        QJsonArray sgEdges = sg["edges"].toArray();
        QCOMPARE(sgEdges.size(), 1);
    }

    // --------------------------------------------------------------------
    // fromTestCase conversion
    // --------------------------------------------------------------------
    void testFromTestCaseConversion()
    {
        TestCase tc = buildTestCase();
        FlowGraph g = FlowGraph::fromTestCase(tc);

        // Basic properties
        QCOMPARE(QString::fromStdString(g.name), QString("BasicMotorTest"));
        QCOMPARE(QString::fromStdString(g.description), QString("Motor start/stop sequence"));

        // Should have 5 nodes (one per step)
        QCOMPARE(static_cast<int>(g.nodes.size()), 5);

        // Node types should map correctly
        QCOMPARE(QString::fromStdString(g.nodes[0].type), QString("StartMotor"));
        QCOMPARE(QString::fromStdString(g.nodes[1].type), QString("Delay"));
        QCOMPARE(QString::fromStdString(g.nodes[2].type), QString("SetParameter"));
        QCOMPARE(QString::fromStdString(g.nodes[3].type), QString("Assert"));
        QCOMPARE(QString::fromStdString(g.nodes[4].type), QString("StopMotor"));

        // Labels should match descriptions
        QCOMPARE(QString::fromStdString(g.nodes[0].label), QString("启动电机"));
        QCOMPARE(QString::fromStdString(g.nodes[1].label), QString("等待2秒"));
        QCOMPARE(QString::fromStdString(g.nodes[2].label), QString("设置转速1000RPM"));

        // Params should be preserved
        QCOMPARE(static_cast<int>(g.nodes[1].params.size()), 1);
        QCOMPARE(QString::fromStdString(g.nodes[1].params[0].first), QString("durationMs"));
        QCOMPARE(QString::fromStdString(g.nodes[1].params[0].second), QString("2000"));

        QCOMPARE(static_cast<int>(g.nodes[2].params.size()), 1);
        QCOMPARE(QString::fromStdString(g.nodes[2].params[0].first), QString("Speed"));

        QCOMPARE(static_cast<int>(g.nodes[3].params.size()), 3);
        QCOMPARE(QString::fromStdString(g.nodes[3].params[0].first), QString("channel"));

        // Should have 4 edges connecting 5 nodes sequentially
        QCOMPARE(static_cast<int>(g.edges.size()), 4);
        QCOMPARE(QString::fromStdString(g.edges[0].fromNodeId), QString::fromStdString(g.nodes[0].id));
        QCOMPARE(QString::fromStdString(g.edges[0].toNodeId),   QString::fromStdString(g.nodes[1].id));
        QCOMPARE(QString::fromStdString(g.edges[3].fromNodeId), QString::fromStdString(g.nodes[3].id));
        QCOMPARE(QString::fromStdString(g.edges[3].toNodeId),   QString::fromStdString(g.nodes[4].id));

        // stopOnFailure should be reflected
        bool foundStopOnFail = false;
        for (const auto& v : g.variables) {
            if (v.find("__stopOnFailure: true") != std::string::npos) {
                foundStopOnFail = true;
                break;
            }
        }
        QVERIFY(foundStopOnFail);
    }

    // --------------------------------------------------------------------
    // fromTestCase: empty test case (no steps)
    // --------------------------------------------------------------------
    void testFromTestCaseEmpty()
    {
        TestCase tc;
        tc.name = "Empty";
        tc.steps.clear();

        FlowGraph g = FlowGraph::fromTestCase(tc);
        QCOMPARE(QString::fromStdString(g.name), QString("Empty"));
        QCOMPARE(static_cast<int>(g.nodes.size()), 0);
        QCOMPARE(static_cast<int>(g.edges.size()), 0);
    }

    // --------------------------------------------------------------------
    // fromTestCase: single step (no edges)
    // --------------------------------------------------------------------
    void testFromTestCaseSingleStep()
    {
        TestCase tc;
        tc.name = "SingleStep";
        TestStep s;
        s.type = StepType::Custom;
        s.description = "Single custom step";
        tc.steps.push_back(s);

        FlowGraph g = FlowGraph::fromTestCase(tc);
        QCOMPARE(static_cast<int>(g.nodes.size()), 1);
        QCOMPARE(static_cast<int>(g.edges.size()), 0);  // no edges for single node
        QCOMPARE(QString::fromStdString(g.nodes[0].type), QString("CustomCommand"));
        QCOMPARE(QString::fromStdString(g.nodes[0].label), QString("Single custom step"));
    }

    // --------------------------------------------------------------------
    // findNode: find existing and non-existing nodes
    // --------------------------------------------------------------------
    void testFindNode()
    {
        FlowGraph g = buildSimpleGraph();

        const FlowNode* found = g.findNode("n1");
        QVERIFY(found != nullptr);
        QCOMPARE(QString::fromStdString(found->type), QString("StartMotor"));

        found = g.findNode("n2");
        QVERIFY(found != nullptr);
        QCOMPARE(QString::fromStdString(found->type), QString("Delay"));

        found = g.findNode("n999");
        QVERIFY(found == nullptr);
    }

    // --------------------------------------------------------------------
    // edgesFrom: get outgoing edges from a node
    // --------------------------------------------------------------------
    void testEdgesFrom()
    {
        FlowGraph g = buildSimpleGraph();

        auto out = g.edgesFrom("n1");
        QCOMPARE(static_cast<int>(out.size()), 1);
        QCOMPARE(QString::fromStdString(out[0]->toNodeId), QString("n2"));
        QCOMPARE(out[0]->fromPort, PortType::Default);

        auto out2 = g.edgesFrom("n2");
        QCOMPARE(static_cast<int>(out2.size()), 0);  // no outgoing edges

        auto out3 = g.edgesFrom("nonexistent");
        QCOMPARE(static_cast<int>(out3.size()), 0);
    }

    // --------------------------------------------------------------------
    // startNode: find the entry point (node with no incoming edges)
    // --------------------------------------------------------------------
    void testStartNode()
    {
        FlowGraph g = buildSimpleGraph();
        const FlowNode* start = g.startNode();
        QVERIFY(start != nullptr);
        QCOMPARE(QString::fromStdString(start->id), QString("n1"));
        QCOMPARE(QString::fromStdString(start->type), QString("StartMotor"));
    }

    // --------------------------------------------------------------------
    // startNode: empty graph returns nullptr
    // --------------------------------------------------------------------
    void testStartNodeEmpty()
    {
        FlowGraph g;
        const FlowNode* start = g.startNode();
        QVERIFY(start == nullptr);
    }

    // --------------------------------------------------------------------
    // startNode: cycle (all nodes have incoming edges) returns first node
    // --------------------------------------------------------------------
    void testStartNodeCycle()
    {
        FlowGraph g;
        g.name = "Cycle";

        FlowNode n1;
        n1.id = "n1"; n1.type = "StartMotor"; n1.label = "N1";
        g.nodes.push_back(n1);

        FlowNode n2;
        n2.id = "n2"; n2.type = "Delay"; n2.label = "N2";
        g.nodes.push_back(n2);

        FlowNode n3;
        n3.id = "n3"; n3.type = "StopMotor"; n3.label = "N3";
        g.nodes.push_back(n3);

        // Cycle: n1→n2, n2→n3, n3→n1
        FlowEdge e1;
        e1.id = "e1"; e1.fromNodeId = "n1"; e1.toNodeId = "n2";
        g.edges.push_back(e1);

        FlowEdge e2;
        e2.id = "e2"; e2.fromNodeId = "n2"; e2.toNodeId = "n3";
        g.edges.push_back(e2);

        FlowEdge e3;
        e3.id = "e3"; e3.fromNodeId = "n3"; e3.toNodeId = "n1";
        g.edges.push_back(e3);

        const FlowNode* start = g.startNode();
        // In a cycle, no node has zero incoming edges — returns first node
        QVERIFY(start != nullptr);
        QCOMPARE(QString::fromStdString(start->id), QString("n1"));
    }

    // --------------------------------------------------------------------
    // PortType: If node with Yes/No ports
    // --------------------------------------------------------------------
    void testIfNodePorts()
    {
        FlowGraph g;
        g.name = "IfTest";

        FlowNode n1;
        n1.id = "n1"; n1.type = "StartMotor"; n1.label = "启动";
        g.nodes.push_back(n1);

        FlowNode nIf;
        nIf.id = "nif"; nIf.type = "If"; nIf.label = "转速>1000?";
        nIf.params = {{"condition", "Speed > 1000"}};
        g.nodes.push_back(nIf);

        FlowNode nYes;
        nYes.id = "nyes"; nYes.type = "Comment"; nYes.label = "是：正常";
        g.nodes.push_back(nYes);

        FlowNode nNo;
        nNo.id = "nno"; nNo.type = "SetParameter"; nNo.label = "否：调速";
        nNo.params = {{"Speed", "1000"}};
        g.nodes.push_back(nNo);

        FlowEdge eYes;
        eYes.id = "eyes"; eYes.fromNodeId = "nif"; eYes.toNodeId = "nyes";
        eYes.fromPort = PortType::Yes;
        g.edges.push_back(eYes);

        FlowEdge eNo;
        eNo.id = "eno"; eNo.fromNodeId = "nif"; eNo.toNodeId = "nno";
        eNo.fromPort = PortType::No;
        g.edges.push_back(eNo);

        // Serialize and verify port types
        QJsonObject json = g.toJson();
        auto parsed = FlowGraph::fromJson(json);
        QVERIFY(parsed.has_value());

        QCOMPARE(static_cast<int>(parsed->edges.size()), 2);
        QCOMPARE(QString::fromStdString(parsed->edges[0].id), QString("eyes"));
        QCOMPARE(parsed->edges[0].fromPort, PortType::Yes);
        QCOMPARE(QString::fromStdString(parsed->edges[1].id), QString("eno"));
        QCOMPARE(parsed->edges[1].fromPort, PortType::No);

        // Verify edgesFrom returns both ports
        auto outEdges = parsed->edgesFrom("nif");
        QCOMPARE(static_cast<int>(outEdges.size()), 2);
    }

    // --------------------------------------------------------------------
    // Serialization: portTypeToString / portTypeFromString helpers
    // --------------------------------------------------------------------
    void testPortTypeHelpers()
    {
        QCOMPARE(QString(portTypeToString(PortType::Default)), QString("default"));
        QCOMPARE(QString(portTypeToString(PortType::Yes)), QString("yes"));
        QCOMPARE(QString(portTypeToString(PortType::No)), QString("no"));

        QCOMPARE(portTypeFromString("default"), PortType::Default);
        QCOMPARE(portTypeFromString("yes"), PortType::Yes);
        QCOMPARE(portTypeFromString("no"), PortType::No);
        QCOMPARE(portTypeFromString("unknown"), PortType::Default);  // fallback
    }
};

QTEST_MAIN(TestFlowGraph)
#include "test_flowgraph.moc"
