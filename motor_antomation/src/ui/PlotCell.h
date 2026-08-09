#pragma once

#include <QWidget>
#include <QVector>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMenu>
#include <QEvent>
#include <QPixmap>
#include <QPainter>
#include <cstdint>

namespace MotorStudio {

class CurveWidget;
class CurveEngine;

// PlotCell — self-contained subplot wrapping a CurveWidget.
//
// Each PlotCell has its own:
//  - Header bar: editable name label, time-sync toggle, close button
//  - Channel bar: per-channel checkboxes with colored dots
//  - CurveWidget: curve rendering area
//  - Resize handle: drag to adjust height
//
// Channels are selectively mapped to CurveEngine topics. The same topic
// can appear in multiple PlotCells; curve colors are globally unified
// via TopicRegistry.

class PlotCell : public QWidget {
    Q_OBJECT
public:
    explicit PlotCell(const QString& name, CurveEngine* engine, QWidget* parent = nullptr);

    QString name() const;
    void setName(const QString& name);
    CurveWidget* curveWidget() const;

    // Channel configuration
    void setChannels(const QVector<uint32_t>& topicIds);  // replace all display channels
    void addChannel(uint32_t topicId);
    void removeChannel(uint32_t topicId);
    QVector<uint32_t> channels() const;

    // Time axis sync
    bool isTimeSynced() const;
    void setTimeSynced(bool sync);

    int preferredHeight() const;
    void setPreferredHeight(int h);

signals:
    void closeRequested();
    void nameChanged(const QString& name);
    void timeSyncChanged(bool synced);
    void heightChanged(int newHeight);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onNameDoubleClicked();
    void onNameEditingFinished();
    void onSyncToggleClicked();
    void onCloseClicked();
    void onChannelCheckToggled(bool checked);
    void onChannelBarContextMenu(const QPoint& pos);

private:
    void setupUi();
    void rebuildChannelBar();
    void appendChannel(uint32_t topicId);  // WF-03: 只追加通道数据，不触发 rebuildChannelBar
    void updateSyncButtonStyle();
    QPixmap makeColorDot(const QColor& color, int size = 10) const;

    // Data
    QString m_name;
    int m_preferredHeight = 250;
    bool m_timeSynced = true;
    QVector<uint32_t> m_channelIds;  // topicIds for channels displayed in this plot

    // Engine reference
    CurveEngine* m_engine = nullptr;

    // Layout
    QVBoxLayout* m_mainLayout = nullptr;

    // Header bar
    QWidget* m_headerBar = nullptr;
    QHBoxLayout* m_headerLayout = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLineEdit* m_nameEdit = nullptr;    // shown during edit, hidden otherwise
    QPushButton* m_syncBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // Channel bar
    QWidget* m_channelBar = nullptr;
    QHBoxLayout* m_channelLayout = nullptr;
    QVector<QCheckBox*> m_channelCheckboxes;
    QLabel* m_addChannelHint = nullptr;  // "+" hint label

    // Curve widget
    CurveWidget* m_curveWidget = nullptr;

    // Resize handle
    QWidget* m_resizeHandle = nullptr;
};

} // namespace MotorStudio
