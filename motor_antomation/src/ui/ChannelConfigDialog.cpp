#include "ChannelConfigDialog.h"
#include "../databus/Topic.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QColorDialog>
#include <QMessageBox>
#include <QLabel>

namespace MotorStudio {

const QColor ChannelConfigDialog::kDefaultColors[] = {
    QColor("#00bcd4"), QColor("#ff6b6b"), QColor("#51cf66"),
    QColor("#ffd43b"), QColor("#cc5de8"), QColor("#ff922b"),
    QColor("#20c997"), QColor("#339af0"), QColor("#f06595"),
    QColor("#94d82d"), QColor("#5c7cfa"), QColor("#f59f00"),
};
const int ChannelConfigDialog::kNumDefaultColors = 12;

ChannelConfigDialog::ChannelConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Channel Configuration"));
    setMinimumSize(700, 400);
    setupUi();
}

void ChannelConfigDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // Header
    auto* header = new QLabel(tr("Configure channel names, units, colors, and scaling factors."));
    header->setWordWrap(true);
    layout->addWidget(header);

    // Table
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Unit"), tr("Type"), tr("Scale"), tr("Offset"), tr("Color")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_table, &QTableWidget::cellChanged, this, &ChannelConfigDialog::onCellChanged);
    connect(m_table, &QTableWidget::cellClicked, this, &ChannelConfigDialog::onColorClicked);
    layout->addWidget(m_table);

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton(tr("Add Channel"));
    m_removeBtn = new QPushButton(tr("Remove Selected"));
    m_applyBtn = new QPushButton(tr("Apply"));
    m_closeBtn = new QPushButton(tr("Close"));

    connect(m_addBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onAddChannel);
    connect(m_removeBtn, &QPushButton::clicked, this, &ChannelConfigDialog::onRemoveChannel);
    connect(m_applyBtn, &QPushButton::clicked, this, &ChannelConfigDialog::applyToRegistry);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    m_applyBtn->setDefault(true);
    m_applyBtn->setStyleSheet("QPushButton { font-weight: bold; }");

    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_applyBtn);
    btnLayout->addWidget(m_closeBtn);
    layout->addLayout(btnLayout);
}

void ChannelConfigDialog::loadFromRegistry()
{
    auto& registry = TopicRegistry::instance();
    auto ids = registry.allTopicIds();

    m_table->blockSignals(true);
    m_table->setRowCount(0);
    m_rows.clear();

    for (size_t i = 0; i < ids.size(); ++i) {
        auto desc = registry.descriptor(ids[i]);
        ChannelRow row;
        row.topicId = ids[i];
        row.name = QString::fromStdString(desc.name);
        row.unit = QString::fromStdString(desc.unit);
        row.dataType = QString::fromStdString(desc.dataType);
        if (row.dataType.isEmpty()) row.dataType = "float";
        row.scale = desc.scale;
        row.offset = desc.offset;
        // Load color from registry descriptor; fall back to palette if unset
        if (desc.color != 0 && desc.color != 0xFF888888) {
            row.color = QColor((desc.color >> 16) & 0xFF, (desc.color >> 8) & 0xFF, desc.color & 0xFF,
                               (desc.color >> 24) & 0xFF);
        } else {
            row.color = (i < kNumDefaultColors) ? kDefaultColors[i] : nextColor();
        }
        m_rows.append(row);

        int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(row.name));
        m_table->setItem(r, 1, new QTableWidgetItem(row.unit));
        m_table->setItem(r, 2, new QTableWidgetItem(row.dataType));

        auto* scaleItem = new QTableWidgetItem(QString::number(row.scale, 'f', 3));
        m_table->setItem(r, 3, scaleItem);

        auto* offsetItem = new QTableWidgetItem(QString::number(row.offset, 'f', 3));
        m_table->setItem(r, 4, offsetItem);

        // Color cell
        auto* colorItem = new QTableWidgetItem();
        colorItem->setBackground(row.color);
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
        colorItem->setToolTip(tr("Click to change color"));
        m_table->setItem(r, 5, colorItem);
    }

    m_table->blockSignals(false);
}

void ChannelConfigDialog::applyToRegistry()
{
    auto& registry = TopicRegistry::instance();

    for (const auto& row : m_rows) {
        auto desc = registry.descriptor(row.topicId);
        if (!desc.name.empty() || desc.topicId == row.topicId) {
            // Allow apply even if descriptor was just registered (name may be default)
            desc.name = row.name.toStdString();
            desc.unit = row.unit.toStdString();
            desc.dataType = row.dataType.toStdString();
            if (desc.dataType.empty()) desc.dataType = "float";
            desc.scale = row.scale;
            desc.offset = row.offset;
            // Persist color as ARGB uint32_t
            QRgb rgba = row.color.rgba();
            desc.color = static_cast<uint32_t>(rgba);
            // registerTopic handles rename via topicId-first lookup
            registry.registerTopic(desc);
        }
    }
}

void ChannelConfigDialog::onCellChanged(int row, int col)
{
    if (row < 0 || row >= m_rows.size()) return;

    auto* item = m_table->item(row, col);
    if (!item) return;

    QString text = item->text();
    switch (col) {
    case 0: m_rows[row].name = text; break;
    case 1: m_rows[row].unit = text; break;
    case 2: m_rows[row].dataType = text; break;
    case 3: m_rows[row].scale = text.toFloat(); break;
    case 4: m_rows[row].offset = text.toFloat(); break;
    }
}

void ChannelConfigDialog::onAddChannel()
{
    int idx = m_rows.size() + 1;
    auto& registry = TopicRegistry::instance();

    // Build a descriptor and register through TopicRegistry to get a real topicId
    ChannelDescriptor desc;
    desc.name = QString("CH%1").arg(idx).toStdString();
    desc.unit = "";
    desc.dataType = "float";
    desc.scale = 1.0f;
    desc.offset = 0.0f;

    QColor col = (idx - 1 < kNumDefaultColors) ? kDefaultColors[idx - 1] : nextColor();
    QRgb rgba = col.rgba();
    desc.color = static_cast<uint32_t>(rgba);

    TopicId tid = registry.registerTopic(desc);

    ChannelRow row;
    row.topicId = tid;
    row.name = QString::fromStdString(desc.name);
    row.unit = "";
    row.dataType = "float";
    row.scale = 1.0f;
    row.offset = 0.0f;
    row.color = col;
    m_rows.append(row);

    int r = m_table->rowCount();
    m_table->blockSignals(true);
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem(row.name));
    m_table->setItem(r, 1, new QTableWidgetItem(row.unit));
    m_table->setItem(r, 2, new QTableWidgetItem(row.dataType));
    m_table->setItem(r, 3, new QTableWidgetItem(QString::number(row.scale, 'f', 3)));
    m_table->setItem(r, 4, new QTableWidgetItem(QString::number(row.offset, 'f', 3)));

    auto* colorItem = new QTableWidgetItem();
    colorItem->setBackground(row.color);
    colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(r, 5, colorItem);
    m_table->blockSignals(false);
}

void ChannelConfigDialog::onRemoveChannel()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_rows.size()) return;

    m_rows.removeAt(row);
    m_table->removeRow(row);
}

void ChannelConfigDialog::onColorClicked(int row, int col)
{
    if (col != 5 || row < 0 || row >= m_rows.size()) return;

    QColor current = m_rows[row].color;
    QColor selected = QColorDialog::getColor(current, this, tr("Select Channel Color"));
    if (selected.isValid()) {
        m_rows[row].color = selected;
        m_table->item(row, col)->setBackground(selected);
    }
}

QColor ChannelConfigDialog::nextColor() const
{
    int idx = m_rows.size() % kNumDefaultColors;
    return kDefaultColors[idx];
}

} // namespace MotorStudio
