#pragma once
#include <QObject>
#include <QMutex>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace MotorStudio {

enum class VarType { Number, Boolean, String };

struct Variable {
    std::string name;
    VarType type = VarType::Number;
    double numberValue = 0.0;
    bool boolValue = false;
    std::string stringValue;
};

class VariableScope : public QObject {
    Q_OBJECT
public:
    explicit VariableScope(QObject* parent = nullptr);

    // Set/get variables
    void setNumber(const std::string& name, double value);
    void setBool(const std::string& name, bool value);
    void setString(const std::string& name, const std::string& value);

    std::optional<double> getNumber(const std::string& name) const;
    std::optional<bool> getBool(const std::string& name) const;
    std::optional<std::string> getString(const std::string& name) const;

    // Check existence
    bool has(const std::string& name) const;
    VarType type(const std::string& name) const;

    // List all variables
    std::vector<std::string> names() const;
    size_t count() const;

    // Clear all
    void clear();

    // Parent scope for nested scopes (sub-flow support)
    VariableScope* parentScope() const;
    void setParentScope(VariableScope* parent);

    // Resolve variable through scope chain (checks self, then parent, then grandparent...)
    std::optional<double> resolveNumber(const std::string& name) const;

signals:
    void variableChanged(const std::string& name);

private:
    std::unordered_map<std::string, Variable> m_vars;
    VariableScope* m_parent = nullptr;
    mutable QMutex m_mutex;
};

} // namespace MotorStudio

Q_DECLARE_METATYPE(MotorStudio::VarType)
