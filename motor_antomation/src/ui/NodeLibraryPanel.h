#pragma once

#include "FlowCanvas.h"

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QVBoxLayout>

namespace MotorStudio {

// ============================================================
// NodeLibraryPanel — left-side palette for dragging nodes into
//                      the FlowCanvas
// ============================================================
class NodeLibraryPanel : public QWidget {
    Q_OBJECT
public:
    explicit NodeLibraryPanel(FlowCanvas* canvas, QWidget* parent = nullptr);

signals:
    void nodeTypeSelected(const QString& type);

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void setupUi();
    void populateTree();

    FlowCanvas*  m_canvas;
    QLineEdit*   m_searchEdit;
    QTreeWidget* m_tree;
};

} // namespace MotorStudio
