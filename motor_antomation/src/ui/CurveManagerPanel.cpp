#include "CurveManagerPanel.h"
#include "CurveWidget.h"
#include "MultiCurveContainer.h"
#include "ChannelConfigDialog.h"
#include "../curve/CurveEngine.h"
#include "../databus/DataBus.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QColorDialog>
#include <QCheckBox>
#include <QMessageBox>

namespace MotorStudio {

CurveManagerPanel::CurveManagerPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void CurveManagerPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // ---- Toolbar row ----
    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(4, 2, 4, 2);

    m_titleLabel = new QLabel(tr("曲线管理器"));
    m_titleLabel->setStyleSheet(
        "color: #2196F3; font-size: 13px; font-weight: bold;"
    );
    toolbar->addWidget(m_titleLabel);
    toolbar->addStretch();

    toolbar->addWidget(new QLabel(tr("窗口:")));
    m_windowCombo = new QComboBox();
    m_windowCombo->setMinimumWidth(90);
    m_windowCombo->setToolTip(tr("通道添加/删除的目标窗口"));
    m_windowCombo->setStyleSheet(
        "QComboBox { background: #FFFFFF; color: #212121; border: 1px solid #E0E0E0;"
        " border-radius: 3px; padding: 2px 6px; }"
        "QComboBox:hover { background: #E0E0E0; }"
        "QComboBox QAbstractItemView { background: #F5F7FA; color: #212121;"
        " selection-background-color: #E0E0E0; }"
    );
    connect(m_windowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CurveManagerPanel::onTargetWindowChanged);
    toolbar->addWidget(m_windowCombo);

    m_addBtn = new QPushButton(tr("+ 添加"));
    m_addBtn->setToolTip(tr("从注册表添加通道到当前窗口"));
    m_addBtn->setStyleSheet(
        "QPushButton { background: #2196F3; color: #ffffff; border: none;"
        " border-radius: 3px; padding: 3px 10px; font-weight: bold; }"
        "QPushButton:hover { background: #1976D2; }"
    );
    connect(m_addBtn, &QPushButton::clicked, this, &CurveManagerPanel::onAddChannel);
    toolbar->addWidget(m_addBtn);

    m_refreshBtn = new QPushButton(tr("刷新"));
    m_refreshBtn->setToolTip(tr("从注册表重新加载通道列表"));
    m_refreshBtn->setStyleSheet(
        "QPushButton { background: #FFFFFF; color: #212121; border: 1px solid #E0E0E0;"
        " border-radius: 3px; padding: 3px 10px; }"
        "QPushButton:hover { background: #E0E0E0; }"
    );
    connect(m_refreshBtn, &QPushButton::clicked, this, &CurveManagerPanel::refresh);
    toolbar->addWidget(m_refreshBtn);

    layout->addLayout(toolbar);

    // ---- Channel table ----
    m_table = new QTableWidget(0, COL_COUNT, this);
    m_table->setHorizontalHeaderLabels({
        tr("名称"), tr("颜色"), tr("单位"),
        tr("Y轴最小"), tr("Y轴最大"), tr("可见"), tr("操作")
    });

    auto* hdr = m_table->horizontalHeader();
    hdr->setStretchLastSection(false);
    hdr->setSectionResizeMode(COL_NAME,    QHeaderView::Stretch);
    hdr->setSectionResizeMode(COL_COLOR,   QHeaderView::Fixed);
    hdr->setSectionResizeMode(COL_UNIT,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(COL_YMIN,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(COL_YMAX,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(COL_VISIBLE, QHeaderView::Fixed);
    hdr->setSectionResizeMode(COL_ACTION,  QHeaderView::Fixed);

    m_table->setColumnWidth(COL_COLOR,   50);
    m_table->setColumnWidth(COL_UNIT,    60);
    m_table->setColumnWidth(COL_YMIN,    70);
    m_table->setColumnWidth(COL_YMAX,    70);
    m_table->setColumnWidth(COL_VISIBLE, 60);
    m_table->setColumnWidth(COL_ACTION,  55);

    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(
        "QTableWidget {"
        "  background-color: #F5F7FA;"
        "  color: #212121;"
        "  gridline-color: #E0E0E0;"
        "  border: 1px solid #E0E0E0;"
        "  font-size: 12px;"
        "  alternate-background-color: #F0F0F0;"
        "}"
        "QTableWidget::item { padding: 2px 4px; }"
        "QTableWidget::item:selected { background-color: #FFFFFF; }"
        "QHeaderView::section {"
        "  background-color: #FFFFFF;"
        "  color: #2196F3;"
        "  border: 1px solid #E0E0E0;"
        "  padding: 3px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
    );

    connect(m_table, &QTableWidget::cellChanged,      this, &CurveManagerPanel::onCellChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &CurveManagerPanel::onCellDoubleClicked);

    layout->addWidget(m_table);
}

// ============================================================
// External wiring
// ============================================================

void CurveManagerPanel::setCurveContainer(MultiCurveContainer* container)
{
    m_curveContainer = container;
    // Populate window combo
    m_windowCombo->blockSignals(true);
    m_windowCombo->clear();
    if (container) {
        int count = container->curveWidgetCount();
        for (int i = 0; i < count; ++i) {
            m_windowCombo->addItem(tr("窗口 %1").arg(i + 1), i);
        }
    }
    m_windowCombo->blockSignals(false);
    loadFromRegistry();
}

void CurveManagerPanel::setCurveEngine(CurveEngine* engine)
{
    m_curveEngine = engine;
}

CurveWidget* CurveManagerPanel::currentCurveWidget() const
{
    if (!m_curveContainer) return nullptr;
    int idx = m_windowCombo->currentData().toInt();
    return m_curveContainer->curveWidgetAt(idx);
}

// ============================================================
// Load table from TopicRegistry
// ============================================================

void CurveManagerPanel::loadFromRegistry()
{
    auto& registry = TopicRegistry::instance();
    auto ids = registry.allTopicIds();

    m_table->blockSignals(true);
    m_table->setRowCount(0);
    m_rows.clear();

    // Helper: pick a default color from the palette
    auto pickColor = [](size_t idx) -> QColor {
        const auto& defaults = ChannelConfigDialog::kDefaultColors;
        return defaults[idx % static_cast<size_t>(ChannelConfigDialog::kNumDefaultColors)];
    };

    for (size_t i = 0; i < ids.size(); ++i) {
        auto desc = registry.descriptor(ids[i]);

        ChannelRow row;
        row.topicId = ids[i];
        row.name    = QString::fromStdString(desc.name);
        row.unit    = QString::fromStdString(desc.unit);
        row.yMin    = -10.0f;
        row.yMax    = 10.0f;
        row.visible = true;

        // Color from registry descriptor
        if (desc.color != 0 && desc.color != 0xFF888888) {
            row.color = QColor(
                (desc.color >> 16) & 0xFF,
                (desc.color >> 8) & 0xFF,
                 desc.color & 0xFF,
                (desc.color >> 24) & 0xFF
            );
        } else {
            row.color = pickColor(i);
        }

        // Pull Y-range and visibility from an existing CurveWidget if available.
        auto* cw = currentCurveWidget();
        if (cw) {
            for (int ci = 0; ci < cw->channelCount(); ++ci) {
                if (cw->channelTopicId(ci) == row.topicId) {
                    row.visible = cw->isChannelVisible(ci);
                    break;
                }
            }
        }

        m_rows.append(row);

        int r = m_table->rowCount();
        m_table->insertRow(r);

        // COL_NAME — editable
        auto* nameItem = new QTableWidgetItem(row.name);
        nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(r, COL_NAME, nameItem);

        // COL_COLOR — click-to-change (non-editable text)
        auto* colorItem = new QTableWidgetItem();
        colorItem->setBackground(row.color);
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
        colorItem->setToolTip(tr("双击修改颜色"));
        m_table->setItem(r, COL_COLOR, colorItem);

        // COL_UNIT — read-only display
        auto* unitItem = new QTableWidgetItem(row.unit);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(r, COL_UNIT, unitItem);

        // COL_YMIN — editable float
        auto* yminItem = new QTableWidgetItem(QString::number(row.yMin, 'f', 1));
        yminItem->setFlags(yminItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(r, COL_YMIN, yminItem);

        // COL_YMAX — editable float
        auto* ymaxItem = new QTableWidgetItem(QString::number(row.yMax, 'f', 1));
        ymaxItem->setFlags(ymaxItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(r, COL_YMAX, ymaxItem);

        // COL_VISIBLE — centered checkbox widget
        auto* checkWidget = new QWidget();
        auto* checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        auto* checkBox = new QCheckBox();
        checkBox->setChecked(row.visible);
        checkBox->setStyleSheet("QCheckBox { spacing: 0px; }");
        // Store topicId as property for stable lookup after row reordering
        quint32 tidForCb = row.topicId;
        checkBox->setProperty("topicId", tidForCb);
        connect(checkBox, &QCheckBox::toggled, this, [this, checkBox](bool checked) {
            quint32 tid = checkBox->property("topicId").toUInt();
            // Find current row index for this topic
            for (int rr = 0; rr < static_cast<int>(m_rows.size()); ++rr) {
                if (m_rows[rr].topicId == tid) {
                    onVisibleToggled(rr, checked);
                    break;
                }
            }
        });
        checkLayout->addWidget(checkBox);
        m_table->setCellWidget(r, COL_VISIBLE, checkWidget);

        // COL_ACTION — delete button
        auto* btnWidget = new QWidget();
        auto* btnLayout = new QHBoxLayout(btnWidget);
        btnLayout->setContentsMargins(2, 1, 2, 1);
        auto* delBtn = new QPushButton(tr("删除"));
        delBtn->setFixedSize(44, 22);
        delBtn->setStyleSheet(
            "QPushButton { background: #F44336; color: #ffffff; border: none;"
            " border-radius: 3px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background: #EF5350; }"
        );
        quint32 tidForDel = row.topicId;
        delBtn->setProperty("topicId", tidForDel);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            quint32 tid = delBtn->property("topicId").toUInt();
            for (int rr = 0; rr < static_cast<int>(m_rows.size()); ++rr) {
                if (m_rows[rr].topicId == tid) {
                    onRemoveChannel(rr);
                    break;
                }
            }
        });
        btnLayout->addWidget(delBtn);
        m_table->setCellWidget(r, COL_ACTION, btnWidget);
    }

    m_table->blockSignals(false);

    // Refresh window combo counts in case tabs were added/removed
    if (m_curveContainer) {
        m_windowCombo->blockSignals(true);
        int prev = m_windowCombo->currentData().toInt();
        m_windowCombo->clear();
        int count = m_curveContainer->curveWidgetCount();
        for (int i = 0; i < count; ++i) {
            m_windowCombo->addItem(tr("窗口 %1").arg(i + 1), i);
        }
        if (prev >= 0 && prev < count) {
            m_windowCombo->setCurrentIndex(prev);
        } else if (count > 0) {
            m_windowCombo->setCurrentIndex(0);
        }
        m_windowCombo->blockSignals(false);
    }
}

void CurveManagerPanel::refresh()
{
    loadFromRegistry();
}

// ============================================================
// Slots
// ============================================================

void CurveManagerPanel::onCellChanged(int row, int col)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size())) return;

    auto* item = m_table->item(row, col);
    if (!item) return;

    QString text = item->text().trimmed();

    switch (col) {
    case COL_NAME: {
        if (text.isEmpty()) return;
        m_rows[row].name = text;
        // Persist to TopicRegistry
        auto& registry = TopicRegistry::instance();
        auto desc = registry.descriptor(m_rows[row].topicId);
        desc.name = text.toStdString();
        registry.registerTopic(desc);
        break;
    }
    case COL_YMIN: {
        bool ok = false;
        float val = text.toFloat(&ok);
        if (!ok) return;
        m_rows[row].yMin = val;
        if (m_rows[row].yMin >= m_rows[row].yMax) {
            m_rows[row].yMax = m_rows[row].yMin + 1.0f;
            m_table->blockSignals(true);
            m_table->item(row, COL_YMAX)->setText(
                QString::number(m_rows[row].yMax, 'f', 1));
            m_table->blockSignals(false);
        }
        // Apply to target CurveWidget's Y range
        auto* cw = currentCurveWidget();
        if (cw) {
            cw->setYRange(m_rows[row].yMin, m_rows[row].yMax);
        }
        break;
    }
    case COL_YMAX: {
        bool ok = false;
        float val = text.toFloat(&ok);
        if (!ok) return;
        m_rows[row].yMax = val;
        if (m_rows[row].yMax <= m_rows[row].yMin) {
            m_rows[row].yMin = m_rows[row].yMax - 1.0f;
            m_table->blockSignals(true);
            m_table->item(row, COL_YMIN)->setText(
                QString::number(m_rows[row].yMin, 'f', 1));
            m_table->blockSignals(false);
        }
        auto* cw = currentCurveWidget();
        if (cw) {
            cw->setYRange(m_rows[row].yMin, m_rows[row].yMax);
        }
        break;
    }
    default:
        break;
    }
}

void CurveManagerPanel::onCellDoubleClicked(int row, int col)
{
    if (col == COL_COLOR && row >= 0 && row < static_cast<int>(m_rows.size())) {
        QColor current = m_rows[row].color;
        QColor selected = QColorDialog::getColor(current, this,
                                                  tr("选择通道颜色"));
        if (!selected.isValid()) return;

        m_rows[row].color = selected;
        m_table->item(row, col)->setBackground(selected);

        // Persist to TopicRegistry
        auto& registry = TopicRegistry::instance();
        auto desc = registry.descriptor(m_rows[row].topicId);
        QRgb rgba = selected.rgba();
        desc.color = static_cast<uint32_t>(rgba);
        registry.registerTopic(desc);

        // Apply to all CurveWidgets immediately
        applyColorToAllCurves(row, selected);
    }
}

void CurveManagerPanel::onAddChannel()
{
    ChannelConfigDialog dlg(this);
    dlg.loadFromRegistry();
    if (dlg.exec() == QDialog::Accepted) {
        dlg.applyToRegistry();

        // Ensure new channels are added to CurveEngine
        auto& registry = TopicRegistry::instance();
        auto ids = registry.allTopicIds();
        if (m_curveEngine) {
            for (auto tid : ids) {
                if (!m_curveEngine->hasChannel(tid)) {
                    m_curveEngine->addChannel(tid);
                }
            }
        }

        loadFromRegistry();
    }
}

void CurveManagerPanel::onRemoveChannel(int row)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size())) return;

    TopicId tid  = m_rows[row].topicId;
    QString name = m_rows[row].name;

    auto result = QMessageBox::question(
        this,
        tr("删除通道"),
        tr("从所有窗口和注册表中删除通道 \"%1\" (ID %2)?")
            .arg(name).arg(tid),
        QMessageBox::Yes | QMessageBox::No
    );

    if (result != QMessageBox::Yes) return;

    // 1. Remove from all CurveWidgets (by matching name)
    applyDeleteToAllCurves(name);

    // 2. Remove from CurveEngine
    if (m_curveEngine) {
        m_curveEngine->removeChannel(tid);
    }

    // 3. Remove from TopicRegistry
    auto& registry = TopicRegistry::instance();
    registry.removeTopic(tid);

    // 4. Update local table
    m_rows.removeAt(row);
    m_table->removeRow(row);
}

void CurveManagerPanel::onVisibleToggled(int row, bool visible)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size())) return;
    m_rows[row].visible = visible;
    applyVisibilityToAllCurves(row, visible);
}

void CurveManagerPanel::onTargetWindowChanged(int /*index*/)
{
    // Refresh the table to reflect the selected window's channel state
    loadFromRegistry();
}

// ============================================================
// Helpers — propagate state to all CurveWidget instances
// ============================================================

void CurveManagerPanel::applyColorToAllCurves(int row, const QColor& color)
{
    if (!m_curveContainer) return;
    uint32_t tid = m_rows[row].topicId;
    for (int i = 0; i < m_curveContainer->curveWidgetCount(); ++i) {
        auto* cw = m_curveContainer->curveWidgetAt(i);
        if (!cw) continue;
        for (int ci = 0; ci < cw->channelCount(); ++ci) {
            if (cw->channelTopicId(ci) == tid) {
                cw->setChannelColor(ci, color);
                cw->update();
                break;
            }
        }
    }
}

void CurveManagerPanel::applyVisibilityToAllCurves(int row, bool visible)
{
    if (!m_curveContainer) return;
    uint32_t tid = m_rows[row].topicId;
    for (int i = 0; i < m_curveContainer->curveWidgetCount(); ++i) {
        auto* cw = m_curveContainer->curveWidgetAt(i);
        if (!cw) continue;
        for (int ci = 0; ci < cw->channelCount(); ++ci) {
            if (cw->channelTopicId(ci) == tid) {
                cw->setChannelVisible(ci, visible);
                break;
            }
        }
    }
}

void CurveManagerPanel::applyDeleteToAllCurves(const QString& channelName)
{
    if (!m_curveContainer) return;
    for (int wi = 0; wi < m_curveContainer->curveWidgetCount(); ++wi) {
        auto* cw = m_curveContainer->curveWidgetAt(wi);
        if (!cw) continue;
        for (int ci = cw->channelCount() - 1; ci >= 0; --ci) {
            if (cw->channelName(ci) == channelName) {
                cw->removeChannel(ci);
                break;
            }
        }
    }
}

} // namespace MotorStudio
