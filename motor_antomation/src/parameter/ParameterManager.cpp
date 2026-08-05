#include "ParameterManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

namespace MotorStudio {

ParameterManager::ParameterManager(QObject* parent)
    : QObject(parent)
{
}

int ParameterManager::addParameter(const ParameterDef& param)
{
    m_params.append(param);
    return m_params.size() - 1;
}

void ParameterManager::removeParameter(int index)
{
    if (index >= 0 && index < m_params.size()) {
        m_params.removeAt(index);
    }
}

void ParameterManager::removeAll()
{
    m_params.clear();
}

const ParameterDef& ParameterManager::parameter(int index) const
{
    static ParameterDef empty;
    if (index >= 0 && index < m_params.size()) return m_params[index];
    return empty;
}

ParameterDef& ParameterManager::parameterRef(int index)
{
    static ParameterDef empty;
    if (index >= 0 && index < m_params.size()) return m_params[index];
    return empty;
}

bool ParameterManager::setValue(int index, const QVariant& value)
{
    if (index < 0 || index >= m_params.size()) return false;
    if (m_params[index].readOnly) return false;
    m_params[index].value = value;
    emit parameterChanged(index, m_params[index].name, value);
    return true;
}

QVariant ParameterManager::value(int index) const
{
    if (index < 0 || index >= m_params.size()) return QVariant();
    return m_params[index].value;
}

bool ParameterManager::setValueByName(const QString& name, const QVariant& value)
{
    for (int i = 0; i < m_params.size(); ++i) {
        if (m_params[i].name == name) {
            return setValue(i, value);
        }
    }
    return false;
}

QVariant ParameterManager::valueByName(const QString& name) const
{
    for (const auto& p : m_params) {
        if (p.name == name) return p.value;
    }
    return QVariant();
}

QJsonObject ParameterManager::toJson() const
{
    QJsonObject root;
    QJsonArray arr;
    for (const auto& p : m_params) {
        QJsonObject obj;
        obj["name"] = p.name;
        obj["displayName"] = p.displayName;
        obj["unit"] = p.unit;
        obj["value"] = QJsonValue::fromVariant(p.value);
        obj["defaultValue"] = QJsonValue::fromVariant(p.defaultValue);
        obj["minValue"] = QJsonValue::fromVariant(p.minValue);
        obj["maxValue"] = QJsonValue::fromVariant(p.maxValue);
        obj["description"] = p.description;
        obj["readOnly"] = p.readOnly;
        arr.append(obj);
    }
    root["parameters"] = arr;
    root["version"] = 1;
    return root;
}

bool ParameterManager::fromJson(const QJsonObject& json)
{
    QJsonArray arr = json["parameters"].toArray();
    m_params.clear();
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        ParameterDef p;
        p.name = obj["name"].toString();
        p.displayName = obj["displayName"].toString();
        p.unit = obj["unit"].toString();
        p.value = obj["value"].toVariant();
        p.defaultValue = obj["defaultValue"].toVariant();
        p.minValue = obj["minValue"].toVariant();
        p.maxValue = obj["maxValue"].toVariant();
        p.description = obj["description"].toString();
        p.readOnly = obj["readOnly"].toBool();
        m_params.append(p);
    }
    emit parametersLoaded();
    return true;
}

bool ParameterManager::saveToFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[ParameterManager] Cannot write:" << filePath;
        return false;
    }
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    emit parametersSaved();
    return true;
}

bool ParameterManager::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ParameterManager] Cannot read:" << filePath;
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return false;
    return fromJson(doc.object());
}

} // namespace MotorStudio