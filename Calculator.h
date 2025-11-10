#ifndef CALCULATOR_H
#define CALCULATOR_H

#include "Spec.h"
#include <QVariant>
#include <QMap>
#include <QString>
#include <functional>

class Calculator {
public:
    using InputMap = QMap<QString, QVariant>; // fieldName -> value
    using Callback = std::function<bool(const ProjectSpec&, const InputMap&, double& out, QString* explain)>;

    Calculator();

    // 计算某项目；优先回调，否则使用公式（QJSEngine）
    bool evaluate(const ProjectSpec& spec, const InputMap& inputs, double& out, QString* explain=nullptr) const;

    // 注册回调（给极复杂项目用）
    void registerCallback(const QString& id, Callback cb);

private:
    QMap<QString, Callback> callbacks_;
};


#endif // CALCULATOR_H
