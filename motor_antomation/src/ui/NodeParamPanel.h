#pragma once

#include "automation/FlowGraph.h"

#include <QWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace MotorStudio {

// ============================================================
// NodeParamPanel — right-side parameter editor for the
//                   currently selected FlowNode
// ============================================================
class NodeParamPanel : public QWidget {
    Q_OBJECT
public:
    explicit NodeParamPanel(QWidget* parent = nullptr);

    void setNode(FlowNode* node);
    void clearNode();

signals:
    void paramsChanged();

private:
    void setupUi();
    void buildForm(const FlowNode& node);
    void clearForm();
    void onAnyParamChanged();
    void onAddParamClicked();
    void onDeleteParam(const QString& key);

    FlowNode* m_currentNode = nullptr;

    QLabel*      m_titleLabel;
    QFormLayout* m_formLayout;
    QWidget*     m_formWidget;
    QScrollArea* m_scrollArea;
    QLabel*      m_emptyLabel;
};

} // namespace MotorStudio
