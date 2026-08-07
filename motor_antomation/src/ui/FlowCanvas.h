#pragma once

#include "automation/FlowGraph.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QColor>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <unordered_map>
#include <string>

namespace MotorStudio {

// ============================================================
// FlowNodeItem — visual node on the canvas
// ============================================================
class FlowNodeItem : public QGraphicsItem
{
public:
    explicit FlowNodeItem(const FlowNode& node, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    QPainterPath shape() const override;

    const FlowNode& node() const { return m_node; }
    void setNode(const FlowNode& node);
    void setHighlighted(bool on);

    // Port positions for edge connection
    QPointF inputPortPos() const;           // top center
    QPointF outputPortPos() const;          // bottom center (Default port)
    QPointF yesPortPos() const;             // bottom-left (Yes port, for If nodes)
    QPointF noPortPos() const;              // bottom-right (No port, for If nodes)

    bool isPortHit(const QPointF& scenePos, PortType* outPort = nullptr) const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    QColor categoryColor() const;
    QString typeDisplayLabel() const;
    QString categoryDisplayName() const;
    bool isIfNode() const;

    FlowNode m_node;
    bool m_highlighted = false;

    static constexpr int NODE_WIDTH  = 160;
    static constexpr int NODE_HEIGHT = 56;
    static constexpr int STRIPE_WIDTH = 4;
    static constexpr int PORT_RADIUS = 5;
    static constexpr int CORNER_RADIUS = 6;
};

// ============================================================
// FlowEdgeItem — bezier curve between two ports
// ============================================================
class FlowEdgeItem : public QGraphicsPathItem
{
public:
    FlowEdgeItem(const FlowEdge& edge, FlowNodeItem* from, FlowNodeItem* to,
                 QGraphicsItem* parent = nullptr);

    const FlowEdge& edge() const { return m_edge; }
    FlowNodeItem* fromNode() const { return m_from; }
    FlowNodeItem* toNode() const { return m_to; }

    void updatePath();

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    QPainterPath shape() const override;

private:
    QPointF sourcePortPos() const;
    QPointF targetPortPos() const;
    void drawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from) const;

    FlowEdge m_edge;
    FlowNodeItem* m_from;
    FlowNodeItem* m_to;
};

// ============================================================
// FlowCanvas — QGraphicsView for editing flow charts
// ============================================================
class FlowCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    explicit FlowCanvas(QWidget* parent = nullptr);

    // Load / save flow graph
    void loadGraph(const FlowGraph& graph);
    FlowGraph toGraph() const;

    // Node palette integration
    void addNodeFromPalette(const std::string& nodeType);

    // Execution highlighting
    void highlightNode(const std::string& nodeId, bool on);
    void clearAllHighlights();

    // Selection
    const FlowNodeItem* selectedNode() const;

    // Clear canvas
    void clearCanvas();

signals:
    void nodeSelected(const std::string& nodeId);
    void nodeDeselected();
    void graphChanged();

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void startEdgeDrag(FlowNodeItem* fromNode, PortType port);
    void finishEdgeDrag(QGraphicsItem* targetItem);
    void cancelEdgeDrag();
    void onSceneSelectionChanged();
    FlowNodeItem* findNodeItem(const std::string& id) const;
    void deleteSelectedItems();
    void editSelectedNode();
    std::string nextNodeId();

    QGraphicsScene* m_scene;
    QGraphicsLineItem*  m_dragLine     = nullptr;
    FlowNodeItem*       m_dragFromNode  = nullptr;
    PortType            m_dragFromPort  = PortType::Default;
    int                 m_nextNodeId    = 1;

    QPointF m_lastMouseScenePos;
    QPointF m_pressScenePos;

    // Context menu
    QMenu* m_contextMenu = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_editAction   = nullptr;
};

} // namespace MotorStudio
