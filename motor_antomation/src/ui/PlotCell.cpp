#include "PlotCell.h"
#include "CurveWidget.h"
#include "../curve/CurveEngine.h"
#include "../curve/TimeAxisManager.h"
#include "../databus/Topic.h"

#include <QApplication>
#include <QFont>
#include <QStyle>
#include <QEvent>
#include <QPainterPath>

namespace MotorStudio {

// ---------------------------------------------------------------------------
// Internal: thin resize handle widget that delegates drag to PlotCell
// ---------------------------------------------------------------------------
class ResizeHandle : public QWidget {
public:
    explicit ResizeHandle(PlotCell* cell, QWidget* parent = nullptr)
        : QWidget(parent), m_cell(cell)
    {
        setFixedHeight(4);
        setCursor(Qt::SizeVerCursor);
        setMouseTracking(true);
    }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragStartY = event->globalY();
            m_dragStartHeight = m_cell->height();
            event->accept();  // 阻止冒泡，避免同时触发区域拖拽滚动
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_dragging) {
            int dy = event->globalY() - m_dragStartY;
            int newH = m_dragStartHeight + dy;
            if (newH < 120) newH = 120;   // minimum height
            if (newH > 800) newH = 800;   // maximum height
            m_cell->setPreferredHeight(newH);
            m_cell->setFixedHeight(newH);
            event->accept();
        }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            m_cell->setPreferredHeight(m_cell->height());
            event->accept();
        }
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#E0E4E8"));
        // Draw grip dots
        p.setPen(QPen(QColor("#B0B8C0"), 1));
        int cx = width() / 2;
        for (int x = cx - 15; x <= cx + 15; x += 10) {
            p.drawPoint(x, 1);
            p.drawPoint(x, 3);
        }
    }
private:
    PlotCell* m_cell;
    bool m_dragging = false;
    int m_dragStartY = 0;
    int m_dragStartHeight = 0;
};

// ===================================================================
// PlotCell
// ===================================================================

PlotCell::PlotCell(const QString& name, CurveEngine* engine, QWidget* parent)
    : QWidget(parent)
    , m_name(name)
    , m_engine(engine)
{
    setupUi();
    setPreferredHeight(m_preferredHeight);
    setFixedHeight(m_preferredHeight);

    // Wire TimeAxisManager — sync enabled by default (WI-103)
    m_curveWidget->setTimeAxisManager(&TimeAxisManager::instance());
    m_curveWidget->setTimeSynced(true);

    // White background with subtle border
    setStyleSheet(
        "PlotCell {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #D8DCE3;"
        "  border-radius: 4px;"
        "}"
    );

    QFont font("Microsoft YaHei", 9);
    font.setStyleStrategy(QFont::PreferAntialias);
    setFont(font);
}

void PlotCell::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // ---- Row 0: Header bar ----
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(28);
    m_headerBar->setStyleSheet(
        "background-color: #EBF0F5;"
        "border-bottom: 1px solid #D0D7DE;"
    );

    m_headerLayout = new QHBoxLayout(m_headerBar);
    m_headerLayout->setContentsMargins(8, 2, 4, 2);
    m_headerLayout->setSpacing(4);

    // Name label (double-click to edit)
    m_nameLabel = new QLabel(m_name, m_headerBar);
    m_nameLabel->setStyleSheet(
        "QLabel {"
        "  font-family: 'Microsoft YaHei';"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "  color: #2C3E50;"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    m_nameLabel->installEventFilter(this);  // for double-click detection

    // Name edit (hidden, shown on double-click)
    m_nameEdit = new QLineEdit(m_name, m_headerBar);
    m_nameEdit->setStyleSheet(
        "QLineEdit {"
        "  font-family: 'Microsoft YaHei';"
        "  font-size: 10px;"
        "  background: #FFFFFF;"
        "  border: 1px solid #90CAF9;"
        "  border-radius: 2px;"
        "  padding: 0 2px;"
        "}"
    );
    m_nameEdit->hide();
    connect(m_nameEdit, &QLineEdit::editingFinished,
            this, &PlotCell::onNameEditingFinished);

    m_headerLayout->addWidget(m_nameLabel);
    m_headerLayout->addWidget(m_nameEdit);
    m_headerLayout->addStretch();

    // Sync toggle button
    m_syncBtn = new QPushButton(m_headerBar);
    m_syncBtn->setFixedSize(36, 20);
    m_syncBtn->setCursor(Qt::PointingHandCursor);
    updateSyncButtonStyle();
    connect(m_syncBtn, &QPushButton::clicked,
            this, &PlotCell::onSyncToggleClicked);
    m_headerLayout->addWidget(m_syncBtn);

    // Close button
    m_closeBtn = new QPushButton(QString::fromUtf8("\xC3\x97"), m_headerBar);  // ×
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "  font-family: 'Microsoft YaHei';"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "  color: #7F8C8D;"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 2px;"
        "}"
        "QPushButton:hover {"
        "  color: #E74C3C;"
        "  background: #FDEDEC;"
        "}"
    );
    connect(m_closeBtn, &QPushButton::clicked,
            this, &PlotCell::onCloseClicked);
    m_headerLayout->addWidget(m_closeBtn);

    m_mainLayout->addWidget(m_headerBar);

    // ---- Row 1: Channel bar ----
    m_channelBar = new QWidget(this);
    m_channelBar->setFixedHeight(22);
    m_channelBar->setStyleSheet(
        "background-color: #F8FAFB;"
        "border-bottom: 1px solid #E8ECF0;"
    );
    m_channelBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_channelBar, &QWidget::customContextMenuRequested,
            this, &PlotCell::onChannelBarContextMenu);

    m_channelLayout = new QHBoxLayout(m_channelBar);
    m_channelLayout->setContentsMargins(6, 1, 6, 1);
    m_channelLayout->setSpacing(4);

    m_addChannelHint = new QLabel(QString::fromUtf8("\xEF\xBC\x8B"), m_channelBar);  // ＋
    m_addChannelHint->setStyleSheet(
        "QLabel {"
        "  font-size: 12px;"
        "  color: #90A4AE;"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    m_addChannelHint->setToolTip(QString::fromUtf8("\xE5\x8F\xB3\xE9\x94\xAE\xE6\xB7\xBB\xE5\x8A\xA0\xE9\x80\x9A\xE9\x81\x93"));  // 右键添加通道
    m_channelLayout->addWidget(m_addChannelHint);
    m_channelLayout->addStretch();

    m_mainLayout->addWidget(m_channelBar);

    // ---- Row 2: CurveWidget ----
    m_curveWidget = new CurveWidget(this);
    m_curveWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Disable auto-population so PlotCell manages channels
    m_curveWidget->setAutoPopulateChannels(false);
    m_mainLayout->addWidget(m_curveWidget, 1);

    // ---- Row 3: Resize handle ----
    m_resizeHandle = new ResizeHandle(this, this);
    m_mainLayout->addWidget(m_resizeHandle);
}

// -------------------------------------------------------------------
// Event filter: detect double-click on name label
// -------------------------------------------------------------------

bool PlotCell::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_nameLabel && event->type() == QEvent::MouseButtonDblClick) {
        onNameDoubleClicked();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// -------------------------------------------------------------------
// Name editing
// -------------------------------------------------------------------

void PlotCell::onNameDoubleClicked()
{
    m_nameLabel->hide();
    m_nameEdit->setText(m_name);
    m_nameEdit->show();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void PlotCell::onNameEditingFinished()
{
    QString text = m_nameEdit->text().trimmed();
    if (!text.isEmpty() && text != m_name) {
        m_name = text;
        m_nameLabel->setText(m_name);
        emit nameChanged(m_name);
    }
    m_nameEdit->hide();
    m_nameLabel->show();
}

// -------------------------------------------------------------------
// Sync toggle
// -------------------------------------------------------------------

void PlotCell::onSyncToggleClicked()
{
    m_timeSynced = !m_timeSynced;
    updateSyncButtonStyle();
    emit timeSyncChanged(m_timeSynced);
}

void PlotCell::updateSyncButtonStyle()
{
    if (m_timeSynced) {
        m_syncBtn->setText(QString::fromUtf8("\xE9\x93\xBE\xE6\x8E\xA5"));  // 链接
        m_syncBtn->setStyleSheet(
            "QPushButton {"
            "  font-family: 'Microsoft YaHei';"
            "  font-size: 8px;"
            "  color: #FFFFFF;"
            "  background-color: #42A5F5;"
            "  border: none;"
            "  border-radius: 2px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #1E88E5;"
            "}"
        );
    } else {
        m_syncBtn->setText(QString::fromUtf8("\xE7\x8B\xAC\xE7\xAB\x8B"));  // 独立
        m_syncBtn->setStyleSheet(
            "QPushButton {"
            "  font-family: 'Microsoft YaHei';"
            "  font-size: 8px;"
            "  color: #546E7A;"
            "  background-color: #ECEFF1;"
            "  border: 1px solid #CFD8DC;"
            "  border-radius: 2px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #E0E0E0;"
            "}"
        );
    }
}

// -------------------------------------------------------------------
// Close
// -------------------------------------------------------------------

void PlotCell::onCloseClicked()
{
    emit closeRequested();
}

// -------------------------------------------------------------------
// Channel bar checkboxes
// -------------------------------------------------------------------

void PlotCell::onChannelCheckToggled(bool checked)
{
    QCheckBox* cb = qobject_cast<QCheckBox*>(sender());
    if (!cb) return;

    uint32_t topicId = cb->property("topicId").toUInt();
    if (topicId == 0) return;

    // Find the CurveWidget channel index for this topicId
    for (int i = 0; i < m_curveWidget->channelCount(); ++i) {
        if (m_curveWidget->channelTopicId(i) == topicId) {
            m_curveWidget->setChannelVisible(i, checked);
            break;
        }
    }
}

void PlotCell::onChannelBarContextMenu(const QPoint& pos)
{
    auto& registry = TopicRegistry::instance();
    auto allIds = registry.allTopicIds();
    if (allIds.empty()) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "  font-family: 'Microsoft YaHei';"
        "  font-size: 9px;"
        "  background: #FFFFFF;"
        "  border: 1px solid #D0D7DE;"
        "}"
        "QMenu::item { padding: 3px 24px 3px 8px; }"
        "QMenu::item:selected { background: #E3F2FD; }"
    );

    for (uint32_t tid : allIds) {
        ChannelDescriptor desc = registry.descriptor(tid);
        QString name = QString::fromStdString(desc.name);
        if (name.isEmpty()) {
            name = QString("CH%1").arg(tid);
        }

        bool alreadyAdded = m_channelIds.contains(tid);

        // Create colored dot icon
        QColor chColor = Qt::gray;
        if (desc.color != 0 && desc.color != 0xFF888888) {
            chColor = QColor((desc.color >> 16) & 0xFF, (desc.color >> 8) & 0xFF,
                             desc.color & 0xFF, (desc.color >> 24) & 0xFF);
        }

        QPixmap dotPix(12, 12);
        dotPix.fill(Qt::transparent);
        {
            QPainter dp(&dotPix);
            dp.setRenderHint(QPainter::Antialiasing);
            dp.setBrush(chColor);
            dp.setPen(Qt::NoPen);
            dp.drawEllipse(1, 1, 10, 10);
        }

        QAction* action = menu.addAction(QIcon(dotPix), name);
        action->setCheckable(true);
        action->setChecked(alreadyAdded);
        action->setData(static_cast<uint32_t>(tid));
    }

    QAction* chosen = menu.exec(m_channelBar->mapToGlobal(pos));
    if (!chosen) return;

    uint32_t tid = chosen->data().toUInt();
    bool wasChecked = chosen->isChecked();
    bool wasAdded = m_channelIds.contains(tid);

    if (wasChecked && !wasAdded) {
        addChannel(tid);
    } else if (!wasChecked && wasAdded) {
        removeChannel(tid);
    }
}

// -------------------------------------------------------------------
// Channel management
// -------------------------------------------------------------------

QString PlotCell::name() const
{
    return m_name;
}

void PlotCell::setName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        m_nameLabel->setText(m_name);
        emit nameChanged(m_name);
    }
}

CurveWidget* PlotCell::curveWidget() const
{
    return m_curveWidget;
}

void PlotCell::setChannels(const QVector<uint32_t>& topicIds)
{
    // Remove all existing channels from CurveWidget
    m_curveWidget->clearAllChannels();
    m_channelIds.clear();

    // WF-03：批量追加，全部完成后只重建一次通道栏（避免逐通道 rebuildChannelBar 的 O(N²) churn）
    for (uint32_t tid : topicIds) {
        appendChannel(tid);
    }
    rebuildChannelBar();
}

void PlotCell::appendChannel(uint32_t topicId)
{
    if (m_channelIds.contains(topicId)) return;

    auto& registry = TopicRegistry::instance();
    ChannelDescriptor desc = registry.descriptor(topicId);

    QString name = QString::fromStdString(desc.name);
    if (name.isEmpty()) {
        name = QString("CH%1").arg(topicId);
    }

    QColor color = Qt::cyan;
    if (desc.color != 0 && desc.color != 0xFF888888) {
        color = QColor((desc.color >> 16) & 0xFF, (desc.color >> 8) & 0xFF,
                       desc.color & 0xFF, (desc.color >> 24) & 0xFF);
    }

    int idx = m_curveWidget->addChannel(name, color);
    m_curveWidget->setChannelTopicId(idx, topicId);

    // Attach engine if not already
    if (m_engine) {
        // Engine may not be attached yet; ensure it is
        if (!m_curveWidget->property("_engineAttached").toBool()) {
            m_curveWidget->attachCurveEngine(m_engine, 30);
            m_curveWidget->setProperty("_engineAttached", true);
        }
    }

    m_channelIds.append(topicId);
}

void PlotCell::addChannel(uint32_t topicId)
{
    if (m_channelIds.contains(topicId)) return;
    appendChannel(topicId);
    rebuildChannelBar();
}

void PlotCell::removeChannel(uint32_t topicId)
{
    int chIdx = m_channelIds.indexOf(topicId);
    if (chIdx < 0) return;

    // Find the CurveWidget channel with this topicId
    for (int i = 0; i < m_curveWidget->channelCount(); ++i) {
        if (m_curveWidget->channelTopicId(i) == topicId) {
            m_curveWidget->removeChannel(i);
            break;
        }
    }

    m_channelIds.removeAt(chIdx);
    rebuildChannelBar();
}

QVector<uint32_t> PlotCell::channels() const
{
    return m_channelIds;
}

void PlotCell::rebuildChannelBar()
{
    // Remove old checkboxes (keep the hint label and stretcher)
    for (QCheckBox* cb : m_channelCheckboxes) {
        m_channelLayout->removeWidget(cb);
        cb->deleteLater();
    }
    m_channelCheckboxes.clear();

    // Also clear layout items that might have become dangling
    // Insert new checkboxes before the hint label
    int insertIdx = 0;  // insert at beginning

    auto& registry = TopicRegistry::instance();

    for (uint32_t tid : m_channelIds) {
        ChannelDescriptor desc = registry.descriptor(tid);
        QString name = QString::fromStdString(desc.name);
        if (name.isEmpty()) {
            name = QString("CH%1").arg(tid);
        }

        QColor chColor = Qt::gray;
        if (desc.color != 0 && desc.color != 0xFF888888) {
            chColor = QColor((desc.color >> 16) & 0xFF, (desc.color >> 8) & 0xFF,
                             desc.color & 0xFF, (desc.color >> 24) & 0xFF);
        }

        QCheckBox* cb = new QCheckBox(m_channelBar);
        cb->setText(name);
        cb->setIcon(QIcon(makeColorDot(chColor, 10)));
        cb->setChecked(true);  // newly added channels are visible by default
        cb->setProperty("topicId", static_cast<uint32_t>(tid));

        cb->setStyleSheet(
            "QCheckBox {"
            "  font-family: 'Microsoft YaHei';"
            "  font-size: 9px;"
            "  color: #37474F;"
            "  background: transparent;"
            "  border: none;"
            "  spacing: 3px;"
            "}"
            "QCheckBox::indicator {"
            "  width: 11px;"
            "  height: 11px;"
            "}"
        );

        connect(cb, &QCheckBox::toggled,
                this, &PlotCell::onChannelCheckToggled);

        m_channelLayout->insertWidget(insertIdx, cb);
        m_channelCheckboxes.append(cb);
        ++insertIdx;
    }

    // Show/hide the hint label based on whether there are channels
    m_addChannelHint->setVisible(m_channelIds.isEmpty());
}

QPixmap PlotCell::makeColorDot(const QColor& color, int size) const
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, size - 2, size - 2);
    return pix;
}

// -------------------------------------------------------------------
// Time sync
// -------------------------------------------------------------------

bool PlotCell::isTimeSynced() const
{
    return m_timeSynced;
}

void PlotCell::setTimeSynced(bool sync)
{
    if (m_timeSynced != sync) {
        m_timeSynced = sync;
        updateSyncButtonStyle();
        // Propagate to internal CurveWidget (WI-103 / WI-104 integration)
        m_curveWidget->setTimeSynced(sync);
        emit timeSyncChanged(m_timeSynced);
    }
}

// -------------------------------------------------------------------
// Height
// -------------------------------------------------------------------

int PlotCell::preferredHeight() const
{
    return m_preferredHeight;
}

void PlotCell::setPreferredHeight(int h)
{
    if (m_preferredHeight != h) {
        m_preferredHeight = h;
        setFixedHeight(h);
        emit heightChanged(h);
    }
}

} // namespace MotorStudio
