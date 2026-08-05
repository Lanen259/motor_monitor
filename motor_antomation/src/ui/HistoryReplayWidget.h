#pragma once
#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QVector>
#include <QMap>
#include <QPointF>

namespace MotorStudio {

// 前向声明
class CurveWidget;

// ============================================================
// 历史数据回放控件
// 支持加载CSV文件，在曲线控件中回放历史数据
// ============================================================
class HistoryReplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistoryReplayWidget(QWidget* parent = nullptr);
    ~HistoryReplayWidget() override;

    // 获取内嵌曲线控件
    CurveWidget* curveWidget() const;

    // 已加载的文件列表
    QStringList loadedFiles() const;

    // 清除所有数据
    void clearAll();

signals:
    void fileLoaded(const QString& path);
    void fileLoadError(const QString& path, const QString& error);
    void allCleared();

private slots:
    void onOpenFile();
    void onChannelFilterChanged(int index);
    void onTimeRangeChanged();

private:
    void setupUI();
    void applyDarkTheme();
    void loadCSV(const QString& path);
    QStringList parseCSVLine(const QString& line) const;
    void refreshCurveDisplay();

    // === 工具栏控件 ===
    QPushButton*  m_openBtn;
    QPushButton*  m_clearBtn;
    QComboBox*    m_channelFilter;
    QLabel*       m_infoLabel;
    QCheckBox*    m_showAllChannels;

    // 时间范围选择
    QDateTimeEdit* m_timeRangeStart;
    QDateTimeEdit* m_timeRangeEnd;
    QPushButton*   m_timeRangeApplyBtn;

    // === 曲线显示 ===
    CurveWidget*  m_curveWidget;

    // === 数据存储 ===
    // 每个CSV文件的数据：通道名 -> 数据点列表
    struct FileData {
        QString filePath;
        QMap<QString, QVector<QPointF>> channels;  // 通道名 -> (时间, 值)
        double t0 = 0.0;  // 第一个时间戳，用于对齐
    };
    QVector<FileData> m_loadedFiles;

    // 合并后的通道数据（用于显示）
    QMap<QString, QVector<QPointF>> m_mergedChannels;

    // 当前选择的通道过滤（索引为 -1 表示全部）
    int m_currentChannelIndex = -1;
};

} // namespace MotorStudio