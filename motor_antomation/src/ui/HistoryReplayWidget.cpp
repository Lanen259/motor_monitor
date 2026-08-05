#include "HistoryReplayWidget.h"
#include "CurveWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QScrollBar>
#include <algorithm>

namespace MotorStudio {

HistoryReplayWidget::HistoryReplayWidget(QWidget* parent)
    : QWidget(parent)
    , m_curveWidget(nullptr)
{
    setupUI();
    applyDarkTheme();
}

HistoryReplayWidget::~HistoryReplayWidget() = default;

CurveWidget* HistoryReplayWidget::curveWidget() const
{
    return m_curveWidget;
}

QStringList HistoryReplayWidget::loadedFiles() const
{
    QStringList files;
    for (const auto& fd : m_loadedFiles) {
        files.append(fd.filePath);
    }
    return files;
}

void HistoryReplayWidget::clearAll()
{
    m_loadedFiles.clear();
    m_mergedChannels.clear();
    m_curveWidget->clearData();
    m_curveWidget->clearAllChannels();
    m_channelFilter->clear();
    m_channelFilter->addItem(tr("全部通道"));
    m_infoLabel->setText(tr("就绪 — 未加载文件"));
    emit allCleared();
}

void HistoryReplayWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ========================================
    // 工具栏
    // ========================================
    auto* toolbar = new QHBoxLayout();

    m_openBtn = new QPushButton(tr("打开 CSV"));
    m_openBtn->setMinimumSize(100, 32);
    m_openBtn->setStyleSheet(
        "QPushButton { font-size: 13px; font-weight: bold; background-color: #1565c0; "
        "color: white; border: none; border-radius: 4px; padding: 4px 14px; }"
        "QPushButton:hover { background-color: #1976d2; }");
    connect(m_openBtn, &QPushButton::clicked, this, &HistoryReplayWidget::onOpenFile);
    toolbar->addWidget(m_openBtn);

    m_clearBtn = new QPushButton(tr("清除"));
    m_clearBtn->setMinimumSize(80, 32);
    m_clearBtn->setStyleSheet(
        "QPushButton { font-size: 13px; background-color: #555; "
        "color: #ccc; border: none; border-radius: 4px; padding: 4px 14px; }"
        "QPushButton:hover { background-color: #666; }");
    connect(m_clearBtn, &QPushButton::clicked, this, &HistoryReplayWidget::clearAll);
    toolbar->addWidget(m_clearBtn);

    toolbar->addSpacing(16);

    // 通道过滤
    toolbar->addWidget(new QLabel(tr("通道:")));
    m_channelFilter = new QComboBox();
    m_channelFilter->setMinimumWidth(150);
    m_channelFilter->addItem(tr("全部通道"));
    m_channelFilter->setStyleSheet(
        "QComboBox { background: #2a2a2a; color: #e0e0e0; border: 1px solid #444; "
        "border-radius: 3px; padding: 4px 8px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #2a2a2a; color: #e0e0e0; }");
    connect(m_channelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryReplayWidget::onChannelFilterChanged);
    toolbar->addWidget(m_channelFilter);

    toolbar->addStretch();

    m_infoLabel = new QLabel(tr("就绪 — 未加载文件"));
    m_infoLabel->setStyleSheet("QLabel { color: #888; font-size: 12px; }");
    toolbar->addWidget(m_infoLabel);

    mainLayout->addLayout(toolbar);

    // ========================================
    // 曲线显示区域
    // ========================================
    m_curveWidget = new CurveWidget();
    m_curveWidget->setMinimumHeight(300);
    m_curveWidget->setYAxisLabel(tr("数值"));
    m_curveWidget->setXAxisLabel(tr("时间"));
    mainLayout->addWidget(m_curveWidget, 1);
}

void HistoryReplayWidget::applyDarkTheme()
{
    setStyleSheet(
        "QWidget { background: #1e1e1e; color: #e0e0e0; }"
        "QGroupBox { font-weight: bold; border: 1px solid #333; border-radius: 6px; "
        "margin-top: 12px; padding-top: 16px; background: #252525; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #90caf9; }"
        "QDateTimeEdit { background: #2a2a2a; color: #e0e0e0; border: 1px solid #444; "
        "border-radius: 3px; padding: 4px 8px; }"
        "QScrollBar:vertical { background: #1a1a1a; width: 10px; border-radius: 5px; }"
        "QScrollBar::handle:vertical { background: #444; border-radius: 5px; min-height: 30px; }");
}

void HistoryReplayWidget::onOpenFile()
{
    QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("打开 CSV 数据文件"), QString(),
        tr("CSV 文件 (*.csv);;所有文件 (*)"));
    if (paths.isEmpty()) return;

    for (const QString& path : paths) {
        loadCSV(path);
    }

    refreshCurveDisplay();
}

void HistoryReplayWidget::onChannelFilterChanged(int index)
{
    m_currentChannelIndex = index - 1;  // -1 = 全部
    refreshCurveDisplay();
}

void HistoryReplayWidget::onTimeRangeChanged()
{
    // 时间范围过滤在后续版本实现
    refreshCurveDisplay();
}

void HistoryReplayWidget::loadCSV(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit fileLoadError(path, tr("无法打开文件"));
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    // 读取表头
    QString headerLine = stream.readLine();
    if (headerLine.isEmpty()) {
        file.close();
        emit fileLoadError(path, tr("文件为空"));
        return;
    }

    QStringList headers = parseCSVLine(headerLine);
    if (headers.size() < 2) {
        file.close();
        emit fileLoadError(path, tr("表头格式不正确"));
        return;
    }

    // 第一列通常是 timestamp
    int timestampCol = 0;
    int firstDataCol = 1;

    // 检查第一列是否是时间戳
    if (headers[0].toLower() == "timestamp" || headers[0].toLower() == "time") {
        timestampCol = 0;
        firstDataCol = 1;
    } else {
        // 没有时间戳列，使用行号作为时间
        timestampCol = -1;
        firstDataCol = 0;
    }

    FileData fd;
    fd.filePath = path;

    // 初始化每个通道的空数据向量
    int dataColCount = headers.size() - firstDataCol;
    for (int i = 0; i < dataColCount; ++i) {
        fd.channels[headers[firstDataCol + i]] = QVector<QPointF>();
    }

    double t0 = -1.0;
    int rowCount = 0;
    int errorCount = 0;

    // 读取数据行
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = parseCSVLine(line);
        if (parts.size() < firstDataCol + 1) {
            errorCount++;
            continue;
        }

        rowCount++;

        // 解析时间戳
        double t = 0.0;
        if (timestampCol >= 0) {
            bool ok = false;
            t = parts[timestampCol].toDouble(&ok);
            if (!ok) {
                // 尝试解析为整数时间戳
                qint64 ts = parts[timestampCol].toLongLong(&ok);
                if (ok) {
                    t = static_cast<double>(ts) / 1000.0;  // 微秒转秒
                }
            }
        } else {
            t = static_cast<double>(rowCount - 1) * 0.001;  // 假设1kHz
        }

        if (t0 < 0) t0 = t;

        // 解析数据列
        for (int i = firstDataCol; i < parts.size() && (i - firstDataCol) < dataColCount; ++i) {
            bool ok = false;
            double value = parts[i].toDouble(&ok);
            if (ok) {
                QString colName = headers[i];
                fd.channels[colName].append(QPointF(t - t0, value));
            }
        }
    }

    file.close();
    fd.t0 = t0;

    m_loadedFiles.append(fd);

    QString info = tr("已加载: %1 (%2 行, %3 通道)")
        .arg(QFileInfo(path).fileName())
        .arg(rowCount)
        .arg(dataColCount);
    m_infoLabel->setText(info);

    emit fileLoaded(path);
    qDebug() << "[HistoryReplay] Loaded" << path << "rows:" << rowCount
             << "channels:" << dataColCount << "errors:" << errorCount;
}

QStringList HistoryReplayWidget::parseCSVLine(const QString& line) const
{
    QStringList result;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        QChar c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current += '"';
                i++;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            result.append(current.trimmed());
            current.clear();
        } else {
            current += c;
        }
    }
    result.append(current.trimmed());
    return result;
}

void HistoryReplayWidget::refreshCurveDisplay()
{
    // 合并所有已加载文件的数据
    m_mergedChannels.clear();
    m_curveWidget->clearAllChannels();

    // 收集所有通道名
    QSet<QString> allChannelNames;
    for (const auto& fd : m_loadedFiles) {
        for (auto it = fd.channels.begin(); it != fd.channels.end(); ++it) {
            allChannelNames.insert(it.key());
        }
    }

    // 应用通道过滤
    QStringList filteredNames;
    if (m_currentChannelIndex < 0) {
        // 显示全部
        filteredNames = allChannelNames.values();
    } else {
        QStringList names = allChannelNames.values();
        if (m_currentChannelIndex < names.size()) {
            filteredNames.append(names[m_currentChannelIndex]);
        }
    }

    // 更新通道过滤器下拉框
    m_channelFilter->blockSignals(true);
    QString currentFilter = m_channelFilter->currentText();
    m_channelFilter->clear();
    m_channelFilter->addItem(tr("全部通道"));
    QStringList sortedNames = allChannelNames.values();
    sortedNames.sort();
    for (const auto& name : sortedNames) {
        m_channelFilter->addItem(name);
    }
    // 恢复选择
    int idx = m_channelFilter->findText(currentFilter);
    if (idx >= 0) {
        m_channelFilter->setCurrentIndex(idx);
    }
    m_channelFilter->blockSignals(false);

    // 预定义颜色表
    static const QColor s_colors[] = {
        QColor(0, 150, 255), QColor(255, 80, 80), QColor(0, 200, 100),
        QColor(255, 180, 0), QColor(160, 80, 255), QColor(0, 200, 200),
        QColor(255, 100, 180), QColor(180, 180, 0), QColor(100, 200, 255),
        QColor(255, 150, 50), QColor(50, 255, 150), QColor(200, 100, 255),
    };
    const int colorCount = sizeof(s_colors) / sizeof(s_colors[0]);

    // 为每个通道创建曲线通道并添加数据
    int colorIdx = 0;
    for (const auto& name : filteredNames) {
        int chIdx = m_curveWidget->addChannel(name, s_colors[colorIdx % colorCount]);
        colorIdx++;

        // 合并数据
        QVector<QPointF> merged;
        for (const auto& fd : m_loadedFiles) {
            if (fd.channels.contains(name)) {
                merged.append(fd.channels[name]);
            }
        }

        // 按时间排序
        std::sort(merged.begin(), merged.end(),
                  [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });

        // 推送数据到曲线
        for (const auto& pt : merged) {
            m_curveWidget->pushData(chIdx, static_cast<float>(pt.y()),
                                    static_cast<uint64_t>(pt.x() * 1000000.0));
        }
    }

    // 更新信息标签
    int fileCount = m_loadedFiles.size();
    int channelCount = filteredNames.size();
    m_infoLabel->setText(tr("共 %1 个文件, %2 个通道").arg(fileCount).arg(channelCount));
}

} // namespace MotorStudio