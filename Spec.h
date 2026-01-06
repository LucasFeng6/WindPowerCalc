/*
 * 规格与输入字段的数据结构定义。
 * 描述各项目的分组、输入项、公式以及用于汇总展示的分组行信息
 */
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
    QString unit;     // 单位
    QString sharedKey; // 共享键，如果多个输入字段有相同的共享键，则它们共享一个值
    QVariant defval; // 默认值
    bool required = false;
    double min = std::numeric_limits<double>::lowest();
    double max = std::numeric_limits<double>::max();
};

struct ProjectSpec {
    QString id;       // 唯一标识
    QString group;    // 分组
    QString label;    // 显示名
    QString unit;     // 单位
    QVector<InputField> inputs;
    QString formula;  // QJSEngine 表达式（若注册回调可空）
    QString note;     // 备注
    bool groupHeader = false; // 若为分组标题行（用于汇总表美化）
};

struct ProjectSpecSet {
    QVector<ProjectSpec> items;           // 含分组内顺序
    QStringList groupsInOrder;            // 展示次序
    QMap<QString, QVector<int>> groupRows;// 分组 -> 索引
};

#endif // SPEC_H
