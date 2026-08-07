#include "VariableScope.h"

namespace MotorStudio {

VariableScope::VariableScope(QObject* parent)
    : QObject(parent)
    , m_parent(nullptr)
{
}

// ============================================================
// Set operations
// ============================================================

void VariableScope::setNumber(const std::string& name, double value)
{
    QMutexLocker lk(&m_mutex);
    auto& var = m_vars[name];
    var.name = name;
    var.type = VarType::Number;
    var.numberValue = value;
    var.boolValue = false;
    var.stringValue.clear();
    lk.unlock();
    emit variableChanged(name);
}

void VariableScope::setBool(const std::string& name, bool value)
{
    QMutexLocker lk(&m_mutex);
    auto& var = m_vars[name];
    var.name = name;
    var.type = VarType::Boolean;
    var.boolValue = value;
    var.numberValue = 0.0;
    var.stringValue.clear();
    lk.unlock();
    emit variableChanged(name);
}

void VariableScope::setString(const std::string& name, const std::string& value)
{
    QMutexLocker lk(&m_mutex);
    auto& var = m_vars[name];
    var.name = name;
    var.type = VarType::String;
    var.stringValue = value;
    var.numberValue = 0.0;
    var.boolValue = false;
    lk.unlock();
    emit variableChanged(name);
}

// ============================================================
// Get operations
// ============================================================

std::optional<double> VariableScope::getNumber(const std::string& name) const
{
    QMutexLocker lk(&m_mutex);
    auto it = m_vars.find(name);
    if (it != m_vars.end()) {
        return it->second.numberValue;
    }
    return std::nullopt;
}

std::optional<bool> VariableScope::getBool(const std::string& name) const
{
    QMutexLocker lk(&m_mutex);
    auto it = m_vars.find(name);
    if (it != m_vars.end()) {
        return it->second.boolValue;
    }
    return std::nullopt;
}

std::optional<std::string> VariableScope::getString(const std::string& name) const
{
    QMutexLocker lk(&m_mutex);
    auto it = m_vars.find(name);
    if (it != m_vars.end()) {
        return it->second.stringValue;
    }
    return std::nullopt;
}

// ============================================================
// Existence and type
// ============================================================

bool VariableScope::has(const std::string& name) const
{
    QMutexLocker lk(&m_mutex);
    return m_vars.find(name) != m_vars.end();
}

VarType VariableScope::type(const std::string& name) const
{
    QMutexLocker lk(&m_mutex);
    auto it = m_vars.find(name);
    if (it != m_vars.end()) {
        return it->second.type;
    }
    // 默认返回Number类型
    return VarType::Number;
}

// ============================================================
// Listing
// ============================================================

std::vector<std::string> VariableScope::names() const
{
    QMutexLocker lk(&m_mutex);
    std::vector<std::string> result;
    result.reserve(m_vars.size());
    for (const auto& kv : m_vars) {
        result.push_back(kv.first);
    }
    return result;
}

size_t VariableScope::count() const
{
    QMutexLocker lk(&m_mutex);
    return m_vars.size();
}

// ============================================================
// Clear
// ============================================================

void VariableScope::clear()
{
    QMutexLocker lk(&m_mutex);
    m_vars.clear();
}

// ============================================================
// Parent scope
// ============================================================

VariableScope* VariableScope::parentScope() const
{
    return m_parent;
}

void VariableScope::setParentScope(VariableScope* parent)
{
    QMutexLocker lk(&m_mutex);
    m_parent = parent;
}

// ============================================================
// Scope chain resolution
// ============================================================

std::optional<double> VariableScope::resolveNumber(const std::string& name) const
{
    const VariableScope* current = this;
    while (current) {
        QMutexLocker lk(&current->m_mutex);
        auto it = current->m_vars.find(name);
        if (it != current->m_vars.end()) {
            return it->second.numberValue;
        }
        lk.unlock();
        current = current->m_parent;
    }
    return std::nullopt;
}

} // namespace MotorStudio
