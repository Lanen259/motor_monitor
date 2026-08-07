#include "FlowCanvas.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <cmath>
#include <sstream>

namespace MotorStudio {

// ============================================================
// Internal helpers
// ============================================================

namespace {

// Map node type string → category display name
QString nodeTypeCategory(const std::string& type)
{
    // 控制 → blue
    if (type == "SetParameter" || type == "StartMotor" || type == "StopMotor"
        || type == "SpeedRamp" || type == "CustomCommand")
        return QStringLiteral("控制");  // 控制
    // 时序 → orange
    if (type == "Delay" || type == "Wait" || type == "Timer")
        return QStringLiteral("时序");  // 时序
    // 逻辑 → purple
    if (type == "If" || type == "While" || type == "For"
        || type == "Switch" || type == "Loop" || type == "Jump")
        return QStringLiteral("逻辑");  // 逻辑
    // 数学 → green
    if (type == "Math" || type == "Calculate" || type == "Expression"
        || type == "AssignVariable")
        return QStringLiteral("数学");  // 数学
    // 通信 → cyan
    if (type == "SendCommand" || type == "SerialSend" || type == "CanSend"
        || type == "WriteRegister")
        return QStringLiteral("通信");  // 通信
    // 数据 → teal
    if (type == "ReadParameter" || type == "RecordData"
        || type == "GetValue" || type == "StoreValue"
        || type == "LogOutput" || type == "ExportData")
        return QStringLiteral("数据");  // 数据
    // 断言 → red
    if (type == "Assert" || type == "AssertEqual" || type == "AssertGreater")
        return QStringLiteral("断言");  // 断言
    // 异常 → pink
    if (type == "ThrowError" || type == "TryCatch" || type == "ErrorHandler"
        || type == "ExceptionHandler")
        return QStringLiteral("异常");  // 异常
    // 流程 → gray (default)
    if (type == "SubFlow" || type == "Comment")
        return QStringLiteral("流程");  // 流程
    return QStringLiteral("流程");  // 流程
}

QColor categoryColorForType(const std::string& type)
{
    const QString cat = nodeTypeCategory(type);
    if (cat == QStringLiteral("控制"))  return QColor("#2196F3");  // 控制 blue
    if (cat == QStringLiteral("时序"))  return QColor("#FF9800");  // 时序 orange
    if (cat == QStringLiteral("逻辑"))  return QColor("#9C27B0");  // 逻辑 purple
    if (cat == QStringLiteral("数学"))  return QColor("#4CAF50");  // 数学 green
    if (cat == QStringLiteral("通信"))  return QColor("#00BCD4");  // 通信 cyan
    if (cat == QStringLiteral("数据"))  return QColor("#009688");  // 数据 teal
    if (cat == QStringLiteral("断言"))  return QColor("#F44336");  // 断言 red
    if (cat == QStringLiteral("异常"))  return QColor("#E91E63");  // 异常 pink
    return QColor("#9E9E9E");  // 流程 gray
}

// Human-readable label for node type
QString typeDisplayName(const std::string& type)
{
    if (type == "SetParameter")  return QStringLiteral("设置参数");   // 设置参数
    if (type == "StartMotor")    return QStringLiteral("启动电机");   // 启动电机
    if (type == "StopMotor")     return QStringLiteral("停止电机");   // 停止电机
    if (type == "SpeedRamp")     return QStringLiteral("速度斜坡");   // 速度斜坡
    if (type == "CustomCommand") return QStringLiteral("自定义命令"); // 自定义命令
    if (type == "Delay")         return QStringLiteral("延时");               // 延时
    if (type == "Wait")          return QStringLiteral("等待条件");   // 等待条件
    if (type == "Timer")         return QStringLiteral("定时器");         // 定时器
    if (type == "If")            return QStringLiteral("条件判断");   // 条件判断
    if (type == "While")         return QStringLiteral("循环");               // 循环
    if (type == "For")           return QStringLiteral("计数循环");   // 计数循环
    if (type == "Loop")          return QStringLiteral("循环块");         // 循环块
    if (type == "Switch")        return QStringLiteral("分支选择");   // 分支选择
    if (type == "Jump")          return QStringLiteral("跳转");               // 跳转
    if (type == "Math")          return QStringLiteral("数学运算");   // 数学运算
    if (type == "Calculate")     return QStringLiteral("计算");               // 计算
    if (type == "Expression")    return QStringLiteral("表达式");         // 表达式
    if (type == "AssignVariable") return QStringLiteral("变量赋值");  // 变量赋值
    if (type == "SendCommand")   return QStringLiteral("发送命令");   // 发送命令
    if (type == "SerialSend")    return QStringLiteral("串口发送");   // 串口发送
    if (type == "CanSend")       return QStringLiteral("CAN发送");           // CAN发送
    if (type == "WriteRegister") return QStringLiteral("写寄存器");   // 写寄存器
    if (type == "ReadParameter") return QStringLiteral("读取参数");   // 读取参数
    if (type == "RecordData")    return QStringLiteral("记录数据");   // 记录数据
    if (type == "GetValue")      return QStringLiteral("获取值");         // 获取值
    if (type == "StoreValue")    return QStringLiteral("存储值");         // 存储值
    if (type == "LogOutput")     return QStringLiteral("日志输出");     // 日志输出
    if (type == "ExportData")    return QStringLiteral("导出数据");     // 导出数据
    if (type == "Assert")        return QStringLiteral("断言检查");   // 断言检查
    if (type == "AssertEqual")   return QStringLiteral("等值断言");   // 等值断言
    if (type == "AssertGreater") return QStringLiteral("大于断言");   // 大于断言
    if (type == "ThrowError")    return QStringLiteral("抛出错误");   // 抛出错误
    if (type == "TryCatch")      return QStringLiteral("异常捕获");   // 异常捕获
    if (type == "ErrorHandler")  return QStringLiteral("错误处理");   // 错误处理
    if (type == "ExceptionHandler") return QStringLiteral("异常处理");// 异常处理
    if (type == "SubFlow")       return QStringLiteral("子流程");         // 子流程
    if (type == "Comment")       return QStringLiteral("注释");               // 注释
    if (type == "Nop")           return QStringLiteral("空操作");         // 空操作
    if (type == "Label")         return QStringLiteral("标签");               // 标签
    // fallback: show raw type string
    return QString::fromStdString(type);
}

} // anonymous namespace

// ============================================================
// FlowNodeItem
// ============================================================

FlowNodeItem::FlowNodeItem(const FlowNode& node, QGraphicsItem* parent)
    : QGraphicsItem(parent), m_node(node)
{
    setFlags(QGraphicsItem::ItemIsMovable
             | QGraphicsItem::ItemIsSelectable
             | QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCacheMode(DeviceCoordinateCache);
    setPos(m_node.posX, m_node.posY);
}

QRectF FlowNodeItem::boundingRect() const
{
    qreal pad = 2.0;  // padding for selection border
    return QRectF(-pad, -pad,
                  NODE_WIDTH + pad * 2, NODE_HEIGHT + pad * 2);
}

QPainterPath FlowNodeItem::shape() const
{
    QPainterPath path;
    path.addRoundedRect(0, 0, NODE_WIDTH, NODE_HEIGHT, CORNER_RADIUS, CORNER_RADIUS);
    return path;
}

QColor FlowNodeItem::categoryColor() const
{
    return categoryColorForType(m_node.type);
}

QString FlowNodeItem::categoryDisplayName() const
{
    return nodeTypeCategory(m_node.type);
}

QString FlowNodeItem::typeDisplayLabel() const
{
    return typeDisplayName(m_node.type);
}

bool FlowNodeItem::isIfNode() const
{
    return m_node.type == "If" || m_node.type == "Switch";
}

void FlowNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/,
                         QWidget* /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);

    QRectF body(0, 0, NODE_WIDTH, NODE_HEIGHT);

    // --- Background ---
    QPainterPath bgPath;
    bgPath.addRoundedRect(body, CORNER_RADIUS, CORNER_RADIUS);
    painter->fillPath(bgPath, QColor("#FFFFFF"));

    // --- Category stripe (left) ---
    QColor stripeCol = categoryColor();
    QPainterPath stripePath;
    stripePath.addRoundedRect(0, 0, STRIPE_WIDTH, NODE_HEIGHT,
                              CORNER_RADIUS, CORNER_RADIUS);
    // Clip to only round the left corners
    QPainterPath stripeClip;
    stripeClip.addRect(0, 0, STRIPE_WIDTH, NODE_HEIGHT);
    QPainterPath roundedLeft;
    roundedLeft.addRoundedRect(0, 0, STRIPE_WIDTH * 2, NODE_HEIGHT,
                               CORNER_RADIUS, CORNER_RADIUS);
    stripeClip &= roundedLeft;
    painter->setClipRect(boundingRect());  // safe default clip
    painter->fillPath(stripeClip, stripeCol);

    // --- Border ---
    QPen borderPen;
    if (m_highlighted) {
        borderPen = QPen(QColor("#FF9800"), 2.5);
    } else if (isSelected()) {
        borderPen = QPen(QColor("#2196F3"), 2.0);
    } else {
        borderPen = QPen(QColor("#E0E0E0"), 1.0);
    }
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(body, CORNER_RADIUS, CORNER_RADIUS);

    // --- Highlight glow ---
    if (m_highlighted) {
        QColor glow("#FF9800");
        glow.setAlpha(30);
        QPen glowPen(glow, 4.0);
        glowPen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(glowPen);
        painter->drawRoundedRect(body.adjusted(-1, -1, 1, 1),
                                 CORNER_RADIUS + 2, CORNER_RADIUS + 2);
    }

    // --- Type label (bold, top) ---
    QFont typeFont("Microsoft YaHei", 10, QFont::Bold);
    painter->setFont(typeFont);
    painter->setPen(QColor("#212121"));

    QString typeText = typeDisplayLabel();
    QRectF typeRect(STRIPE_WIDTH + 8, 5, NODE_WIDTH - STRIPE_WIDTH - 16, 20);
    painter->drawText(typeRect, Qt::AlignLeft | Qt::AlignVCenter, typeText);

    // --- Category badge (small tag on top-right) ---
    QFont catFont("Microsoft YaHei", 7);
    painter->setFont(catFont);

    QString catText = categoryDisplayName();
    QFontMetrics fmCat(catFont);
    int catW = fmCat.horizontalAdvance(catText) + 10;
    int catH = 15;
    QRectF catRect(NODE_WIDTH - catW - 6, 6, catW, catH);

    QColor catBg = categoryColor();
    catBg.setAlpha(20);
    painter->setPen(Qt::NoPen);
    painter->setBrush(catBg);
    painter->drawRoundedRect(catRect, 4, 4);

    painter->setPen(categoryColor());
    painter->drawText(catRect, Qt::AlignCenter, catText);

    // --- Node name / description (below type) ---
    QFont descFont("Microsoft YaHei", 8);
    painter->setFont(descFont);
    painter->setPen(QColor("#757575"));

    QString descText;
    if (!m_node.label.empty()) {
        descText = QString::fromStdString(m_node.label);
    } else {
        descText = QString::fromStdString(m_node.id);
    }

    QRectF descRect(STRIPE_WIDTH + 8, 25, NODE_WIDTH - STRIPE_WIDTH - 16, 20);
    // Elide if too long
    QFontMetrics fmDesc(descFont);
    if (fmDesc.horizontalAdvance(descText) > descRect.width()) {
        descText = fmDesc.elidedText(descText, Qt::ElideRight,
                                     static_cast<int>(descRect.width()));
    }
    painter->drawText(descRect, Qt::AlignLeft | Qt::AlignVCenter, descText);

    // --- Input port dot (top center) ---
    painter->setPen(QPen(QColor("#757575"), 1.5));
    painter->setBrush(QColor("#757575"));
    painter->drawEllipse(QPointF(NODE_WIDTH / 2.0, 0), PORT_RADIUS, PORT_RADIUS);

    // --- Output port(s) ---
    if (isIfNode()) {
        // Yes port — bottom-left, green
        painter->setPen(QPen(QColor("#4CAF50"), 1.5));
        painter->setBrush(QColor("#4CAF50"));
        qreal yesX = NODE_WIDTH * 0.3;
        painter->drawEllipse(QPointF(yesX, NODE_HEIGHT), PORT_RADIUS, PORT_RADIUS);

        // Yes label
        QFont portFont("Microsoft YaHei", 7);
        painter->setFont(portFont);
        painter->setPen(QColor("#4CAF50"));
        painter->drawText(QRectF(yesX - 12, NODE_HEIGHT - PORT_RADIUS - 12, 24, 10),
                          Qt::AlignCenter, QStringLiteral("是"));  // 是

        // No port — bottom-right, red
        painter->setPen(QPen(QColor("#F44336"), 1.5));
        painter->setBrush(QColor("#F44336"));
        qreal noX = NODE_WIDTH * 0.7;
        painter->drawEllipse(QPointF(noX, NODE_HEIGHT), PORT_RADIUS, PORT_RADIUS);

        painter->setPen(QColor("#F44336"));
        painter->drawText(QRectF(noX - 12, NODE_HEIGHT - PORT_RADIUS - 12, 24, 10),
                          Qt::AlignCenter, QStringLiteral("否"));  // 否
    } else {
        // Default output port — bottom center, blue
        painter->setPen(QPen(QColor("#2196F3"), 1.5));
        painter->setBrush(QColor("#2196F3"));
        painter->drawEllipse(QPointF(NODE_WIDTH / 2.0, NODE_HEIGHT),
                             PORT_RADIUS, PORT_RADIUS);
    }
}

void FlowNodeItem::setNode(const FlowNode& node)
{
    m_node = node;
    update();
}

void FlowNodeItem::setHighlighted(bool on)
{
    if (m_highlighted != on) {
        m_highlighted = on;
        update();
    }
}

QPointF FlowNodeItem::inputPortPos() const
{
    return mapToScene(QPointF(NODE_WIDTH / 2.0, 0));
}

QPointF FlowNodeItem::outputPortPos() const
{
    return mapToScene(QPointF(NODE_WIDTH / 2.0, NODE_HEIGHT));
}

QPointF FlowNodeItem::yesPortPos() const
{
    return mapToScene(QPointF(NODE_WIDTH * 0.3, NODE_HEIGHT));
}

QPointF FlowNodeItem::noPortPos() const
{
    return mapToScene(QPointF(NODE_WIDTH * 0.7, NODE_HEIGHT));
}

bool FlowNodeItem::isPortHit(const QPointF& scenePos, PortType* outPort) const
{
    QPointF local = mapFromScene(scenePos);

    // Input port (top center)
    QPointF inPort(NODE_WIDTH / 2.0, 0);
    if (QLineF(local, inPort).length() <= PORT_RADIUS + 3.0) {
        if (outPort) *outPort = PortType::Default;
        return true;
    }

    // Output port(s)
    if (isIfNode()) {
        QPointF yesPort(NODE_WIDTH * 0.3, NODE_HEIGHT);
        if (QLineF(local, yesPort).length() <= PORT_RADIUS + 3.0) {
            if (outPort) *outPort = PortType::Yes;
            return true;
        }
        QPointF noPort(NODE_WIDTH * 0.7, NODE_HEIGHT);
        if (QLineF(local, noPort).length() <= PORT_RADIUS + 3.0) {
            if (outPort) *outPort = PortType::No;
            return true;
        }
    } else {
        QPointF outPortPt(NODE_WIDTH / 2.0, NODE_HEIGHT);
        if (QLineF(local, outPortPt).length() <= PORT_RADIUS + 3.0) {
            if (outPort) *outPort = PortType::Default;
            return true;
        }
    }

    return false;
}

bool FlowNodeItem::outputPortHit(const QPointF& scenePos, PortType* outPort) const
{
    QPointF local = mapFromScene(scenePos);

    // Output port(s)
    if (isIfNode()) {
        QPointF yesPort(NODE_WIDTH * 0.3, NODE_HEIGHT);
        if (QLineF(local, yesPort).length() <= PORT_RADIUS + 3.0) {
            if (outPort) *outPort = PortType::Yes;
            return true;
        }
        QPointF noPort(NODE_WIDTH * 0.7, NODE_HEIGHT);
        if (QLineF(local, noPort).length() <= PORT_RADIUS + 3.0) {
            if (outPort) *outPort = PortType::No;
            return true;
        }
    } else {
        QPointF outPortPt(NODE_WIDTH / 2.0, NODE_HEIGHT);
        if (QLineF(local, outPortPt).length() <= PORT_RADIUS + 3.0) {
            if (outPort) *outPort = PortType::Default;
            return true;
        }
    }

    return false;
}

bool FlowNodeItem::inputPortHit(const QPointF& scenePos) const
{
    QPointF local = mapFromScene(scenePos);
    QPointF inPort(NODE_WIDTH / 2.0, 0);
    return QLineF(local, inPort).length() <= PORT_RADIUS + 3.0;
}

QVariant FlowNodeItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged) {
        // Update the underlying data model
        m_node.posX = pos().x();
        m_node.posY = pos().y();

        // 同步刷新场景 + 更新与本节点相连的边。
        // 原来的实现用 QueuedConnection 逐次投递 update，拖动时会把事件队列灌满；
        // 改为直接同步 update()（Qt 自动合并重绘），避免交互卡顿。
        if (auto* sc = scene()) {
            sc->update();
            const QList<QGraphicsItem*> all = sc->items();
            for (auto* e : all) {
                if (auto* edgeItem = dynamic_cast<FlowEdgeItem*>(e)) {
                    if (edgeItem->fromNode() == this || edgeItem->toNode() == this) {
                        edgeItem->updatePath();
                    }
                }
            }
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void FlowNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* /*event*/)
{
    // Trigger editing via parent canvas
    if (auto* sc = scene()) {
        // The canvas handles this via selectionChanged
    }
}

void FlowNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    // Forward to scene so canvas can show context menu
    setSelected(true);
    QGraphicsItem::contextMenuEvent(event);
}

// ============================================================
// FlowEdgeItem
// ============================================================

FlowEdgeItem::FlowEdgeItem(const FlowEdge& edge, FlowNodeItem* from, FlowNodeItem* to,
                           QGraphicsItem* parent)
    : QGraphicsPathItem(parent), m_edge(edge), m_from(from), m_to(to)
{
    setFlags(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
    setZValue(-1);  // edges behind nodes
    updatePath();
}

QPointF FlowEdgeItem::sourcePortPos() const
{
    if (!m_from) return QPointF();
    switch (m_edge.fromPort) {
        case PortType::Yes: return m_from->yesPortPos();
        case PortType::No:  return m_from->noPortPos();
        default:            return m_from->outputPortPos();
    }
}

QPointF FlowEdgeItem::targetPortPos() const
{
    if (!m_to) return QPointF();
    return m_to->inputPortPos();
}

void FlowEdgeItem::updatePath()
{
    if (!m_from || !m_to) return;

    QPointF src = sourcePortPos();
    QPointF dst = targetPortPos();

    // Cubic bezier with vertical control-point offset
    qreal dy = std::abs(dst.y() - src.y()) * 0.5;
    if (dy < 50.0) dy = 50.0;

    QPointF ctrl1(src.x(), src.y() + dy);
    QPointF ctrl2(dst.x(), dst.y() - dy);

    QPainterPath path;
    path.moveTo(src);
    path.cubicTo(ctrl1, ctrl2, dst);
    setPath(path);
}

void FlowEdgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/,
                         QWidget* /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QColor lineColor = isSelected() ? QColor("#2196F3") : QColor("#757575");

    QPen pen(lineColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    // Arrow head at the target end
    if (!path().isEmpty()) {
        QPointF tip = targetPortPos();
        // Get direction from the second-last control point
        qreal t = 0.95;
        QPointF before = path().pointAtPercent(t);
        drawArrowHead(painter, tip, before);
    }
}

QPainterPath FlowEdgeItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);  // wider hit area for easier selection
    return stroker.createStroke(path());
}

void FlowEdgeItem::drawArrowHead(QPainter* painter, const QPointF& tip,
                                 const QPointF& from) const
{
    QColor arrowColor = isSelected() ? QColor("#2196F3") : QColor("#757575");

    qreal arrowSize = 8.0;
    QLineF line(tip, from);
    if (line.length() < 0.1) return;

    double angle = std::atan2(-line.dy(), line.dx());

    QPointF p1 = tip + QPointF(std::sin(angle - M_PI / 3.0) * arrowSize,
                                std::cos(angle - M_PI / 3.0) * arrowSize);
    QPointF p2 = tip + QPointF(std::sin(angle - M_PI + M_PI / 3.0) * arrowSize,
                                std::cos(angle - M_PI + M_PI / 3.0) * arrowSize);

    QPolygonF arrowHead;
    arrowHead << tip << p1 << p2;

    painter->setPen(Qt::NoPen);
    painter->setBrush(arrowColor);
    painter->drawPolygon(arrowHead);
}

// ============================================================
// FlowCanvas
// ============================================================

FlowCanvas::FlowCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-5000, -5000, 10000, 10000);
    setScene(m_scene);

    // View settings
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                   | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(SmartViewportUpdate);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);

    // Background
    setBackgroundBrush(QColor("#F5F7FA"));

    // Context menu
    m_contextMenu = new QMenu(this);
    m_editAction   = m_contextMenu->addAction(QStringLiteral("编辑参数"));   // 编辑参数
    m_deleteAction = m_contextMenu->addAction(QStringLiteral("删除节点"));   // 删除节点

    connect(m_editAction, &QAction::triggered, this, [this]() { editSelectedNode(); });
    connect(m_deleteAction, &QAction::triggered, this, [this]() { deleteSelectedItems(); });

    // Style context menu
    m_contextMenu->setStyleSheet(R"(
        QMenu {
            background-color: #FFFFFF;
            border: 1px solid #E0E0E0;
            border-radius: 4px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px;
            color: #212121;
            font-family: 'Microsoft YaHei';
            font-size: 12px;
        }
        QMenu::item:selected {
            background-color: #E3F2FD;
            color: #2196F3;
        }
    )");

    // Selection handling
    connect(m_scene, &QGraphicsScene::selectionChanged,
            this, &FlowCanvas::onSceneSelectionChanged);

    // Enable focus for key events
    setFocusPolicy(Qt::StrongFocus);
}

// ============================================================
// Grid background
// ============================================================

void FlowCanvas::drawBackground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawBackground(painter, rect);

    painter->setPen(QPen(QColor("#E8E8E8"), 0.5));

    qreal gridSize = 20.0;

    qreal left   = std::floor(rect.left()   / gridSize) * gridSize;
    qreal top    = std::floor(rect.top()    / gridSize) * gridSize;
    qreal right  = rect.right();
    qreal bottom = rect.bottom();

    for (qreal x = left; x <= right; x += gridSize) {
        painter->drawLine(QPointF(x, top), QPointF(x, bottom));
    }
    for (qreal y = top; y <= bottom; y += gridSize) {
        painter->drawLine(QPointF(left, y), QPointF(right, y));
    }

    // Slightly darker dots at grid intersections
    painter->setPen(QPen(QColor("#D0D0D0"), 1.0));
    for (qreal x = left; x <= right; x += gridSize) {
        for (qreal y = top; y <= bottom; y += gridSize) {
            painter->drawPoint(QPointF(x, y));
        }
    }
}

// ============================================================
// Load / save
// ============================================================

void FlowCanvas::loadGraph(const FlowGraph& graph)
{
    clearCanvas();

    std::unordered_map<std::string, FlowNodeItem*> itemMap;
    m_nextNodeId = 1;

    // Create node items
    for (const auto& node : graph.nodes) {
        auto* item = new FlowNodeItem(node);
        item->setPos(node.posX, node.posY);
        m_scene->addItem(item);
        itemMap[node.id] = item;

        // Track max node id number for nextNodeId
        if (node.id.size() > 1 && node.id[0] == 'n') {
            try {
                int num = std::stoi(node.id.substr(1));
                if (num >= m_nextNodeId) {
                    m_nextNodeId = num + 1;
                }
            } catch (...) {}
        }
    }

    // Create edge items
    for (const auto& edge : graph.edges) {
        auto fromIt = itemMap.find(edge.fromNodeId);
        auto toIt   = itemMap.find(edge.toNodeId);
        if (fromIt != itemMap.end() && toIt != itemMap.end()) {
            auto* edgeItem = new FlowEdgeItem(edge, fromIt->second, toIt->second);
            m_scene->addItem(edgeItem);
        }
    }
}

FlowGraph FlowCanvas::toGraph() const
{
    FlowGraph graph;
    graph.name = "flow";
    graph.description = "";

    int nodeIdx = 1;
    int edgeIdx = 1;
    std::unordered_map<FlowNodeItem*, std::string> nodeIdMap;

    // Collect all node items
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (auto* item : allItems) {
        if (auto* nodeItem = dynamic_cast<FlowNodeItem*>(item)) {
            FlowNode node = nodeItem->node();
            node.posX = nodeItem->pos().x();
            node.posY = nodeItem->pos().y();

            // Ensure unique id
            if (node.id.empty()) {
                std::ostringstream oss;
                oss << "n" << (nodeIdx++);
                node.id = oss.str();
            }
            nodeIdMap[nodeItem] = node.id;
            graph.nodes.push_back(std::move(node));
        }
    }

    // Collect all edge items
    for (auto* item : allItems) {
        if (auto* edgeItem = dynamic_cast<FlowEdgeItem*>(item)) {
            FlowEdge edge = edgeItem->edge();

            // Resolve from/to ids from the connected node items
            FlowNodeItem* from = edgeItem->fromNode();
            FlowNodeItem* to   = edgeItem->toNode();

            auto fromIt = nodeIdMap.find(from);
            auto toIt   = nodeIdMap.find(to);

            if (fromIt != nodeIdMap.end() && toIt != nodeIdMap.end()) {
                edge.fromNodeId = fromIt->second;
                edge.toNodeId   = toIt->second;

                if (edge.id.empty()) {
                    std::ostringstream oss;
                    oss << "e" << (edgeIdx++);
                    edge.id = oss.str();
                }
                graph.edges.push_back(std::move(edge));
            }
        }
    }

    return graph;
}

// ============================================================
// Palette integration
// ============================================================

void FlowCanvas::addNodeFromPalette(const std::string& nodeType)
{
    FlowNode node;
    node.id    = nextNodeId();
    node.type  = nodeType;
    node.label = typeDisplayName(nodeType).toStdString();

    // Default params for known types
    if (nodeType == "SetParameter") {
        node.params.emplace_back("name", "");
        node.params.emplace_back("value", "0");
    } else if (nodeType == "Delay") {
        node.params.emplace_back("ms", "1000");
    } else if (nodeType == "If") {
        node.params.emplace_back("expression", "");
    } else if (nodeType == "Switch") {
        node.params.emplace_back("expression", "");
    } else if (nodeType == "Assert") {
        node.params.emplace_back("expression", "");
        node.params.emplace_back("message", "");
    } else if (nodeType == "Wait") {
        node.params.emplace_back("ms", "1000");
    } else if (nodeType == "For") {
        node.params.emplace_back("var", "i");
        node.params.emplace_back("from", "0");
        node.params.emplace_back("to", "10");
    } else if (nodeType == "SendCommand") {
        node.params.emplace_back("command", "");
    } else if (nodeType == "Math") {
        node.params.emplace_back("expression", "");
    } else if (nodeType == "SpeedRamp") {
        node.params.emplace_back("from", "0");
        node.params.emplace_back("to", "1000");
        node.params.emplace_back("rampMs", "1000");
    } else if (nodeType == "Timer") {
        node.params.emplace_back("interval", "1000");
        node.params.emplace_back("repeat", "单次");
    } else if (nodeType == "Jump") {
        node.params.emplace_back("target", "");
    } else if (nodeType == "AssignVariable") {
        node.params.emplace_back("var", "");
        node.params.emplace_back("expression", "");
    } else if (nodeType == "WriteRegister") {
        node.params.emplace_back("address", "0x00");
        node.params.emplace_back("value", "0");
    } else if (nodeType == "LogOutput") {
        node.params.emplace_back("level", "INFO");
        node.params.emplace_back("message", "");
    } else if (nodeType == "ExportData") {
        node.params.emplace_back("format", "CSV");
        node.params.emplace_back("path", "");
    } else if (nodeType == "ExceptionHandler") {
        node.params.emplace_back("message", "");
        node.params.emplace_back("action", "中断流程");
    } else if (nodeType == "SubFlow") {
        node.params.emplace_back("subflow", "");
    } else if (nodeType == "Comment") {
        node.params.emplace_back("text", "");
    }

    // Place at center of view with slight offset
    QPointF viewCenter = mapToScene(viewport()->rect().center());
    node.posX = viewCenter.x() - FlowNodeItem::NODE_WIDTH / 2.0 + (m_nextNodeId % 5) * 30.0;
    node.posY = viewCenter.y() + (m_nextNodeId % 5) * 40.0;

    auto* item = new FlowNodeItem(node);
    m_scene->addItem(item);

    // Select the new node
    m_scene->clearSelection();
    item->setSelected(true);

    emit graphChanged();
}

// ============================================================
// ID generation
// ============================================================

std::string FlowCanvas::nextNodeId()
{
    std::ostringstream oss;
    oss << "n" << (m_nextNodeId++);
    return oss.str();
}

// ============================================================
// Execution highlighting
// ============================================================

void FlowCanvas::highlightNode(const std::string& nodeId, bool on)
{
    if (auto* item = findNodeItem(nodeId)) {
        item->setHighlighted(on);
        if (on) {
            // Scroll to the highlighted node
            centerOn(item);
        }
    }
}

void FlowCanvas::clearAllHighlights()
{
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (auto* item : allItems) {
        if (auto* nodeItem = dynamic_cast<FlowNodeItem*>(item)) {
            nodeItem->setHighlighted(false);
        }
    }
}

// ============================================================
// Selection
// ============================================================

const FlowNodeItem* FlowCanvas::selectedNode() const
{
    QList<QGraphicsItem*> sel = m_scene->selectedItems();
    for (auto* item : sel) {
        if (auto* nodeItem = dynamic_cast<FlowNodeItem*>(item)) {
            return nodeItem;
        }
    }
    return nullptr;
}

void FlowCanvas::clearCanvas()
{
    m_scene->clear();
    m_dragLine     = nullptr;
    m_dragFromNode = nullptr;
    m_nextNodeId   = 1;
}

// ============================================================
// Internal helpers
// ============================================================

FlowNodeItem* FlowCanvas::findNodeItem(const std::string& id) const
{
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (auto* item : allItems) {
        if (auto* nodeItem = dynamic_cast<FlowNodeItem*>(item)) {
            if (nodeItem->node().id == id) {
                return nodeItem;
            }
        }
    }
    return nullptr;
}

FlowNodeItem* FlowCanvas::findNodeAtOutputPort(const QPointF& scenePos, PortType* port) const
{
    // 遍历场景所有节点做形状无关的端口命中检测。
    // m_scene->items() 按堆叠顺序返回（最上层在前），重叠时优先最上层节点。
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (auto* item : allItems) {
        if (auto* nodeItem = dynamic_cast<FlowNodeItem*>(item)) {
            if (nodeItem->outputPortHit(scenePos, port)) {
                return nodeItem;
            }
        }
    }
    return nullptr;
}

FlowNodeItem* FlowCanvas::findNodeAtTarget(const QPointF& scenePos) const
{
    // 1) 任意端口命中（形状无关）
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (auto* item : allItems) {
        if (auto* nodeItem = dynamic_cast<FlowNodeItem*>(item)) {
            if (nodeItem->inputPortHit(scenePos)
                || nodeItem->outputPortHit(scenePos, nullptr)) {
                return nodeItem;
            }
        }
    }
    // 2) 回退到节点身体命中
    QGraphicsItem* hit = m_scene->itemAt(scenePos, transform());
    return dynamic_cast<FlowNodeItem*>(hit);
}

void FlowCanvas::deleteSelectedItems()
{
    QList<QGraphicsItem*> sel = m_scene->selectedItems();
    if (sel.isEmpty()) return;

    // Block signals during batch delete
    m_scene->blockSignals(true);

    for (auto* item : sel) {
        // Remove both nodes and edges
        if (dynamic_cast<FlowNodeItem*>(item) || dynamic_cast<FlowEdgeItem*>(item)) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    m_scene->blockSignals(false);

    // Now also delete any edge whose source or target was deleted
    QList<QGraphicsItem*> remaining = m_scene->items();
    QList<FlowEdgeItem*> orphanEdges;
    for (auto* item : remaining) {
        if (auto* edgeItem = dynamic_cast<FlowEdgeItem*>(item)) {
            if (!edgeItem->fromNode() || !edgeItem->toNode()) {
                orphanEdges.append(edgeItem);
            }
            // Also check if the node items are still in the scene
            bool fromInScene = false;
            bool toInScene = false;
            for (auto* other : remaining) {
                if (other == edgeItem->fromNode()) fromInScene = true;
                if (other == edgeItem->toNode())   toInScene = true;
            }
            if (!fromInScene || !toInScene) {
                orphanEdges.append(edgeItem);
            }
        }
    }
    for (auto* edge : orphanEdges) {
        m_scene->removeItem(edge);
        delete edge;
    }

    emit graphChanged();
}

void FlowCanvas::editSelectedNode()
{
    QList<QGraphicsItem*> sel = m_scene->selectedItems();
    FlowNodeItem* nodeItem = nullptr;
    for (auto* item : sel) {
        if (auto* ni = dynamic_cast<FlowNodeItem*>(item)) {
            nodeItem = ni;
            break;
        }
    }
    if (!nodeItem) return;

    FlowNode node = nodeItem->node();

    bool ok = false;
    QString newLabel = QInputDialog::getText(
        this,
        QStringLiteral("编辑节点"),             // 编辑节点
        QStringLiteral("节点名称:"),             // 节点名称:
        QLineEdit::Normal,
        QString::fromStdString(node.label),
        &ok);

    if (ok) {
        node.label = newLabel.toStdString();
        nodeItem->setNode(node);
        emit graphChanged();
    }
}

// ============================================================
// Scene selection callback
// ============================================================

void FlowCanvas::onSceneSelectionChanged()
{
    const FlowNodeItem* sel = selectedNode();
    if (sel) {
        emit nodeSelected(sel->node().id);
    } else {
        emit nodeDeselected();
    }
}

// ============================================================
// Mouse events — edge dragging
// ============================================================

void FlowCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressScenePos = mapToScene(event->pos());

        // 只从"输出端口"开始拖拽连线。
        // 用遍历节点做形状无关的端口命中检测：itemAt() 在端口（节点边界）处
        // 常因 shape() 边界判断而返回不到节点，导致连不上线。
        PortType port = PortType::Default;
        if (auto* nodeItem = findNodeAtOutputPort(m_pressScenePos, &port)) {
            startEdgeDrag(nodeItem, port);
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void FlowCanvas::mouseMoveEvent(QMouseEvent* event)
{
    m_lastMouseScenePos = mapToScene(event->pos());

    // Update drag line
    if (m_dragLine && m_dragFromNode) {
        QPointF srcPos;
        switch (m_dragFromPort) {
            case PortType::Yes: srcPos = m_dragFromNode->yesPortPos(); break;
            case PortType::No:  srcPos = m_dragFromNode->noPortPos();  break;
            default:            srcPos = m_dragFromNode->outputPortPos(); break;
        }
        m_dragLine->setLine(QLineF(srcPos, m_lastMouseScenePos));
    }

    QGraphicsView::mouseMoveEvent(event);
}

void FlowCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragLine) {
        QPointF releasePos = mapToScene(event->pos());

        // 松开在任意节点（身体或输入端口）上都算连线，容错更好
        if (auto* targetNode = findNodeAtTarget(releasePos)) {
            finishEdgeDrag(targetNode);
            return;
        }

        // Didn't land on a valid node — cancel
        cancelEdgeDrag();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void FlowCanvas::startEdgeDrag(FlowNodeItem* fromNode, PortType port)
{
    m_dragFromNode = fromNode;
    m_dragFromPort = port;

    QPointF srcPos;
    switch (port) {
        case PortType::Yes: srcPos = fromNode->yesPortPos(); break;
        case PortType::No:  srcPos = fromNode->noPortPos();  break;
        default:            srcPos = fromNode->outputPortPos(); break;
    }

    m_dragLine = new QGraphicsLineItem();
    m_dragLine->setPen(QPen(QColor("#2196F3"), 2.0, Qt::DashLine));
    m_dragLine->setZValue(100);
    m_dragLine->setLine(QLineF(srcPos, srcPos));
    m_scene->addItem(m_dragLine);
}

void FlowCanvas::finishEdgeDrag(FlowNodeItem* toNode)
{
    if (!m_dragFromNode || !toNode) {
        cancelEdgeDrag();
        return;
    }

    if (toNode == m_dragFromNode) {
        cancelEdgeDrag();
        return;
    }

    // Check if an edge already exists between these two nodes
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (auto* item : allItems) {
        if (auto* existing = dynamic_cast<FlowEdgeItem*>(item)) {
            if (existing->fromNode() == m_dragFromNode
                && existing->toNode() == toNode
                && existing->edge().fromPort == m_dragFromPort) {
                // Duplicate edge — skip
                cancelEdgeDrag();
                return;
            }
        }
    }

    // Create new edge
    FlowEdge edge;
    std::ostringstream oss;
    oss << "e" << (m_nextNodeId++);
    edge.id         = oss.str();
    edge.fromNodeId = m_dragFromNode->node().id;
    edge.toNodeId   = toNode->node().id;
    edge.fromPort   = m_dragFromPort;

    auto* edgeItem = new FlowEdgeItem(edge, m_dragFromNode, toNode);
    m_scene->addItem(edgeItem);

    // Clean up drag line
    cancelEdgeDrag();
    emit graphChanged();
}

void FlowCanvas::cancelEdgeDrag()
{
    if (m_dragLine) {
        m_scene->removeItem(m_dragLine);
        delete m_dragLine;
        m_dragLine = nullptr;
    }
    m_dragFromNode = nullptr;
}

// ============================================================
// Key events
// ============================================================

void FlowCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelectedItems();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        cancelEdgeDrag();
        m_scene->clearSelection();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

// ============================================================
// Wheel zoom
// ============================================================

void FlowCanvas::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double scaleFactor = 1.15;
        if (event->angleDelta().y() > 0) {
            scale(scaleFactor, scaleFactor);
        } else {
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

// ============================================================
// Context menu
// ============================================================

void FlowCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    // Check if there's an item under cursor
    QGraphicsItem* item = scene()->itemAt(mapToScene(event->pos()), transform());
    if (dynamic_cast<FlowNodeItem*>(item)) {
        // Select the node
        if (!item->isSelected()) {
            m_scene->clearSelection();
            item->setSelected(true);
        }

        m_editAction->setEnabled(true);
        m_deleteAction->setEnabled(true);
        m_contextMenu->popup(event->globalPos());
    } else {
        // No node under cursor
        m_editAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
        m_contextMenu->popup(event->globalPos());
    }
}

} // namespace MotorStudio
