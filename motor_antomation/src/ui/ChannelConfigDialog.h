#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVector>
#include <cstdint>
#include "../databus/Topic.h"

namespace MotorStudio {

// Dialog for configuring channel properties (name, unit, color, scale, offset)
class ChannelConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChannelConfigDialog(QWidget* parent = nullptr);

    // Load channel list from TopicRegistry
    void loadFromRegistry();

    // Apply changes back to registry
    void applyToRegistry();

    static const QColor kDefaultColors[];
    static const int kNumDefaultColors;

private slots:
    void onCellChanged(int row, int col);
    void onAddChannel();
    void onRemoveChannel();
    void onColorClicked(int row, int col);

private:
    void setupUi();

    QTableWidget* m_table = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    struct ChannelRow {
        TopicId topicId;
        QString name;
        QString unit;
        QString dataType;
        QColor color;
        float scale;
        float offset;
    };
    QVector<ChannelRow> m_rows;

    QColor nextColor() const;
};

} // namespace MotorStudio
