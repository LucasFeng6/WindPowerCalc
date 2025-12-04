#include "Calculator.h"
#include <QJSEngine>
#include <QJSValue>

Calculator::Calculator() {
    // 示例：可以在此注册极复杂项目的回调（默认本版本全部走表达式）
}

bool Calculator::evaluate(const ProjectSpec& spec, const InputMap& inputs, double& out) const {

    QJSEngine eng;
    // 把输入注入JS 全局变量
    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
        // JS 变量名 = 字段名
        eng.globalObject().setProperty(it.key(), QJSValue(it.value().toDouble()));
    }
    // 预置 result=0，确保result属性存在
    eng.globalObject().setProperty("result", 0.0);

    const QString code = spec.formula;
    const QJSValue r = eng.evaluate(code);
    if (r.isError()) {
        return false;
    }
    const QJSValue rv = eng.globalObject().property("result");
    if (!rv.isNumber()) {
        return false;
    }
    out = rv.toNumber();
    return true;
}

