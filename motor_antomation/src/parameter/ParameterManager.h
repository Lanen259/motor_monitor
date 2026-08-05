#pragma once

#include <QObject>
#include <QWidget>
#include <QString>
#include <QVector>
#include <QVariant>
#include <QJsonObject>
#include <QScrollArea>

namespace MotorStudio {

// ============================================================
// 单个参数定义
// ============================================================
struct ParameterDef {
    QString name;        // 参数名
    QString displayName; // 显示名称
    QString unit;        // 单位
    QVariant value;      // 当前值
    QVariant defaultValue;
    QVariant minValue;
    QVariant maxValue;
    QString description;
    bool readOnly = false;
};

// ============================================================
// 参数管理器
// 支持参数读写、JSON导入导出、信号通知
// ============================================================
class ParameterManager : public QObject {
    Q_OBJECT
public:
    explicit ParameterManager(QObject* parent = nullptr);

    // 参数管理
    int addParameter(const ParameterDef& param);
    void removeParameter(int index);
    void removeAll();

    int parameterCount() const { return m_params.size(); }
    const ParameterDef& parameter(int index) const;
    ParameterDef& parameterRef(int index);

    // 参数读写
    bool setValue(int index, const QVariant& value);
    QVariant value(int index) const;
    bool setValueByName(const QString& name, const QVariant& value);
    QVariant valueByName(const QString& name) const;

    // 批量操作
    QVector<ParameterDef> allParameters() const { return m_params; }
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& json);

    // 文件操作
    bool saveToFile(const QString& filePath);
    bool loadFromFile(const QString& filePath);

signals:
    void parameterChanged(int index, const QString& name, const QVariant& value);
    void parametersLoaded();
    void parametersSaved();

private:
    QVector<ParameterDef> m_params;
};

// ============================================================
// 参数编辑面板
// ============================================================
class ParameterWidget : public QWidget {
    Q_OBJECT
public:
    explicit ParameterWidget(QWidget* parent = nullptr);

    void setParameterManager(ParameterManager* mgr);
    void refresh();

    void saveToFile();
    void loadFromFile();

private:
    ParameterManager* m_mgr;
    QWidget* m_contentWidget;
};

} // namespace MotorStudio