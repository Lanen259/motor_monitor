#include "NodeLibraryPanel.h"

#include <QPainter>
#include <QPixmap>
#include <QLabel>

namespace MotorStudio {

// ============================================================
// Internal — category data structure
// ============================================================
struct CategoryEntry {
    QString displayName;
    QColor  color;
    QVector<QPair<QString, QString>> items;  // (displayName, internalType)
};

static const QVector<CategoryEntry>& categoryData()
{
    static QVector<CategoryEntry> data = {
        {
            QStringLiteral("控制"),
            QColor("#2196F3"),
            {
                { QStringLiteral("设置参数"),   QStringLiteral("SetParameter")   },
                { QStringLiteral("启动电机"),   QStringLiteral("StartMotor")     },
                { QStringLiteral("停止电机"),   QStringLiteral("StopMotor")      },
                { QStringLiteral("速度斜坡"),   QStringLiteral("SpeedRamp")      },
                { QStringLiteral("自定义命令"),   QStringLiteral("CustomCommand")  },
            }
        },
        {
            QStringLiteral("时序"),
            QColor("#FF9800"),
            {
                { QStringLiteral("延时"),       QStringLiteral("Delay")          },
                { QStringLiteral("等待条件"),   QStringLiteral("Wait")           },
                { QStringLiteral("定时器"),     QStringLiteral("Timer")          },
            }
        },
        {
            QStringLiteral("逻辑"),
            QColor("#9C27B0"),
            {
                { QStringLiteral("判断 if"),    QStringLiteral("If")             },
                { QStringLiteral("循环 loop"),   QStringLiteral("While")          },
                { QStringLiteral("分支 switch"), QStringLiteral("Switch")         },
                { QStringLiteral("跳转"),       QStringLiteral("Jump")           },
            }
        },
        {
            QStringLiteral("数学"),
            QColor("#4CAF50"),
            {
                { QStringLiteral("计算"),       QStringLiteral("Calculate")      },
                { QStringLiteral("变量赋值"),   QStringLiteral("AssignVariable") },
            }
        },
        {
            QStringLiteral("通信"),
            QColor("#00BCD4"),
            {
                { QStringLiteral("读参数"),     QStringLiteral("ReadParameter")  },
                { QStringLiteral("写寄存器"),   QStringLiteral("WriteRegister")  },
            }
        },
        {
            QStringLiteral("数据"),
            QColor("#009688"),
            {
                { QStringLiteral("记录数据"),   QStringLiteral("RecordData")     },
                { QStringLiteral("日志输出"),   QStringLiteral("LogOutput")     },
                { QStringLiteral("导出"),       QStringLiteral("ExportData")     },
            }
        },
        {
            QStringLiteral("断言"),
            QColor("#F44336"),
            {
                { QStringLiteral("断言"),       QStringLiteral("Assert")         },
            }
        },
        {
            QStringLiteral("异常"),
            QColor("#E91E63"),
            {
                { QStringLiteral("异常处理"),   QStringLiteral("ExceptionHandler") },
            }
        },
        {
            QStringLiteral("流程"),
            QColor("#9E9E9E"),
            {
                { QStringLiteral("子流程"),     QStringLiteral("SubFlow")        },
                { QStringLiteral("注释"),       QStringLiteral("Comment")        },
            }
        },
    };
    return data;
}

// ============================================================
// Helper — create a small colored circle icon
// ============================================================
static QIcon makeCircleIcon(const QColor& color, int size = 16)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(2, 2, size - 4, size - 4);

    painter.end();
    return QIcon(pix);
}

// ============================================================
// Constructor
// ============================================================
NodeLibraryPanel::NodeLibraryPanel(FlowCanvas* canvas, QWidget* parent)
    : QWidget(parent), m_canvas(canvas)
{
    setupUi();
}

void NodeLibraryPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // --- Title ---
    auto* titleLabel = new QLabel(QStringLiteral("节点库"), this);
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-family: 'Microsoft YaHei';
            font-size: 13px;
            font-weight: bold;
            color: #212121;
            padding: 4px 8px;
        }
    )");
    layout->addWidget(titleLabel);

    // --- Search box ---
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索节点..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(R"(
        QLineEdit {
            font-family: 'Microsoft YaHei';
            font-size: 12px;
            color: #212121;
            background-color: #FFFFFF;
            border: 1px solid #E0E0E0;
            border-radius: 4px;
            padding: 6px 8px;
        }
        QLineEdit:focus {
            border-color: #2196F3;
        }
    )");
    layout->addWidget(m_searchEdit);

    // --- Tree widget ---
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(14);
    m_tree->setIconSize(QSize(16, 16));
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setStyleSheet(R"(
        QTreeWidget {
            font-family: 'Microsoft YaHei';
            font-size: 12px;
            color: #212121;
            background-color: #FFFFFF;
            border: none;
            outline: none;
        }
        QTreeWidget::item {
            padding: 4px 6px;
            border-radius: 3px;
            color: #212121;
        }
        QTreeWidget::item:hover {
            background-color: #E3F2FD;
        }
        QTreeWidget::item:selected {
            background-color: #BBDEFB;
            color: #1565C0;
        }
    )");
    layout->addWidget(m_tree);

    // --- Populate ---
    populateTree();

    // --- Connections ---
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &NodeLibraryPanel::onSearchTextChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &NodeLibraryPanel::onItemDoubleClicked);

    // --- Panel background ---
    setStyleSheet(R"(
        NodeLibraryPanel {
            background-color: #FAFBFC;
        }
    )");
}

// ============================================================
// Populate the tree from category data
// ============================================================
void NodeLibraryPanel::populateTree()
{
    m_tree->clear();

    for (const auto& cat : categoryData()) {
        // Category header item
        auto* catItem = new QTreeWidgetItem();
        catItem->setText(0, cat.displayName);
        catItem->setIcon(0, makeCircleIcon(cat.color));
        catItem->setFlags(Qt::ItemIsEnabled);

        QColor catBg = cat.color;
        catBg.setAlpha(18);

        // Custom font/pen for category header
        QFont catFont("Microsoft YaHei", 12, QFont::Bold);
        catItem->setFont(0, catFont);
        catItem->setForeground(0, cat.color);
        catItem->setBackground(0, catBg);
        catItem->setSizeHint(0, QSize(0, 30));

        // Leaf items
        for (const auto& entry : cat.items) {
            auto* leafItem = new QTreeWidgetItem(catItem);
            leafItem->setText(0, entry.first);                       // display name
            leafItem->setData(0, Qt::UserRole, entry.second);       // internal type string
            leafItem->setToolTip(0, entry.first);
            leafItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            leafItem->setSizeHint(0, QSize(0, 26));
        }

        m_tree->addTopLevelItem(catItem);
    }

    // Expand all by default
    m_tree->expandAll();
}

// ============================================================
// Search / filter
// ============================================================
void NodeLibraryPanel::onSearchTextChanged(const QString& text)
{
    const QString filter = text.trimmed();

    // Iterate all items and hide/show based on filter
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* catItem = m_tree->topLevelItem(i);
        int visibleChildren = 0;

        for (int j = 0; j < catItem->childCount(); ++j) {
            QTreeWidgetItem* child = catItem->child(j);
            bool match = filter.isEmpty()
                         || child->text(0).contains(filter, Qt::CaseInsensitive);
            child->setHidden(!match);
            if (match) ++visibleChildren;
        }

        // Show/hide category based on whether it has visible children
        catItem->setHidden(visibleChildren == 0 && !filter.isEmpty());

        // Auto-expand categories that have matches
        if (!filter.isEmpty() && visibleChildren > 0) {
            catItem->setExpanded(true);
        }
    }
}

// ============================================================
// Double-click → add node to canvas
// ============================================================
void NodeLibraryPanel::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    // Ignore category headers
    if (!item || item->childCount() > 0) return;

    const QString nodeType = item->data(0, Qt::UserRole).toString();
    if (nodeType.isEmpty()) return;

    // Add to canvas
    if (m_canvas) {
        m_canvas->addNodeFromPalette(nodeType.toStdString());
    }

    emit nodeTypeSelected(nodeType);
}

} // namespace MotorStudio
