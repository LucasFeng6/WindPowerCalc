#include "Calculator.h"
#include <QJSEngine>
#include <QJSValue>
#include <QStringList>

Calculator::Calculator() {
    // 示例：可以在此注册极复杂项目的回调（默认本版本全部走表达式）
}

void Calculator::registerCallback(const QString& id, Callback cb) {
    callbacks_[id] = std::move(cb);
}

bool Calculator::evaluate(const ProjectSpec& spec, const InputMap& inputs, double& out, QString* explain) const {
    if (callbacks_.contains(spec.id)) {
        return callbacks_.value(spec.id)(spec, inputs, out, explain);
    }

    // 表达式走 QJSEngine
    QJSEngine eng;
    // 把输入注入上下文
    QStringList parts;
    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
        // JS 变量名 = 字段名
        eng.globalObject().setProperty(it.key(), QJSValue(it.value().toDouble()));
        parts << QString("%1=%2").arg(it.key()).arg(it.value().toString());
    }
    // 预置 result=0，确保result属性存在
    eng.globalObject().setProperty("result", 0.0);

    const QString code = spec.formula;
    const QJSValue r = eng.evaluate(code);
    if (r.isError()) {
        if (explain) *explain = QString("表达式错误: %1").arg(r.toString());
        return false;
    }
    const QJSValue rv = eng.globalObject().property("result");
    if (!rv.isNumber()) {
        if (explain) *explain = "表达式未设置 result 或类型非法";
        return false;
    }
    out = rv.toNumber();
    if (explain) {
        *explain = QString("公式: %1\n输入: %2\n结果(result): %3 %4")
                       .arg(code).arg(parts.join(", ")).arg(out).arg(spec.unit);
    }
    return true;
}

