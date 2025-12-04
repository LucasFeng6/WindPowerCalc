#ifndef CALCULATOR_H
#define CALCULATOR_H

#include "Spec.h"
#include <QVariant>
#include <QMap>
#include <QString>


class Calculator {
public:
    using InputMap = QMap<QString, QVariant>; // fieldName -> value

    Calculator();

    // 计算某项目；只使用公式（QJSEngine）
    bool evaluate(const ProjectSpec& spec, const InputMap& inputs, double& out) const;
};


#endif // CALCULATOR_H
