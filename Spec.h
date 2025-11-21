#ifndef SPEC_H
#define SPEC_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QMap>

struct InputField {
    QString name;
    QString label;
    QString type;     // "double" | "int" 
    QString unit;     // e.g. "km", "Ω/km"
    QString sharedKey; // optional: fields with same sharedKey share one value across projects
    QVariant defval;
    bool required = false;
    double min = std::numeric_limits<double>::lowest();
    double max = std::numeric_limits<double>::max();
    QStringList enumOptions;      // for type=enum
};

struct ProjectSpec {
    QString id;       // unique
    QString group;    // 分组
    QString label;    // 显示名
    QString unit;     // 结果单位（如：万元/年）
    QVector<InputField> inputs;
    QString formula;  // QJSEngine 表达式（可空，若注册了回调）
    QString note;     // 备注
    bool groupHeader = false; // 若为分组标题行（用于汇总表美化）
};

struct ProjectSpecSet {
    QVector<ProjectSpec> items;           // 含分组内顺序
    QStringList groupsInOrder;            // 展示次序
    QMap<QString, QVector<int>> groupRows;// group -> indices
};

#endif // SPEC_H
