#include "HistoryReplayWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>

namespace MotorStudio {

HistoryReplayWidget::HistoryReplayWidget(QWidget* parent)
    : QWidget(parent)
    , m_curveWidget(nullptr)
    , m_openBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_channelFilter(nullptr)
    , m_infoLabel(nullptr)
    , m_showAllChannels(nullptr)
    , m_timeRangeStart(nullptr)
    , m_timeRangeEnd(nullptr)
    , m_timeRangeApplyBtn(nullptr)
    , m_currentChannelIndex(-1)
{
    setupUI();
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
    emit allCleared();
}

void HistoryReplayWidget::onOpenFile()
{
    // TODO: 后续版本实现CSV加载
}

void HistoryReplayWidget::onChannelFilterChanged(int /*index*/)
{
    // TODO: 后续版本实现通道过滤
}

void HistoryReplayWidget::onTimeRangeChanged()
{
    // TODO: 后续版本实现时间范围过滤
}

void HistoryReplayWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 工具栏
    auto* toolbar = new QHBoxLayout();
    m_openBtn = new QPushButton(tr("打开文件"));
    m_clearBtn = new QPushButton(tr("清除"));
    m_channelFilter = new QComboBox();
    m_channelFilter->addItem(tr("全部通道"));
    m_showAllChannels = new QCheckBox(tr("显示全部"));
    m_showAllChannels->setChecked(true);
    m_infoLabel = new QLabel(tr("就绪"));

    toolbar->addWidget(m_openBtn);
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_channelFilter);
    toolbar->addWidget(m_showAllChannels);
    toolbar->addStretch();
    toolbar->addWidget(m_infoLabel);
    mainLayout->addLayout(toolbar);

    // 曲线控件占位
    auto* placeholder = new QLabel(tr("历史数据回放 — 曲线控件将在后续版本集成"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("QLabel { color: gray; font-size: 14px; }");
    mainLayout->addWidget(placeholder);

    // 连接信号
    connect(m_openBtn, &QPushButton::clicked, this, &HistoryReplayWidget::onOpenFile);
    connect(m_clearBtn, &QPushButton::clicked, this, &HistoryReplayWidget::clearAll);
    connect(m_channelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryReplayWidget::onChannelFilterChanged);
}

void HistoryReplayWidget::applyDarkTheme()
{
    // TODO: 后续版本实现暗色主题
}

void HistoryReplayWidget::loadCSV(const QString& /*path*/)
{
    // TODO: 后续版本实现CSV加载
}

QStringList HistoryReplayWidget::parseCSVLine(const QString& /*line*/) const
{
    // TODO: 后续版本实现CSV解析
    return {};
}

void HistoryReplayWidget::refreshCurveDisplay()
{
    // TODO: 后续版本实现曲线刷新
}

} // namespace MotorStudio