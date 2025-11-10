#ifndef EDITORPANEL_H
#define EDITORPANEL_H

#include "Spec.h"
#include <QWidget>
#include <QMap>

class QFormLayout;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;

class EditorPanel : public QWidget {
    Q_OBJECT
public:
    explicit EditorPanel(QWidget* parent=nullptr);

    // 切换到指定项目规格，并以给定 inputs 填充（为空则用默认值）
    void setProject(const ProjectSpec& spec, const QMap<QString,QVariant>& curInputs);

    // 读取当前表单内容为 inputs；返回校验是否通过，err 若非空带出错误信息
    bool collectInputs(QMap<QString,QVariant>& out, QString* err=nullptr) const;

signals:
    void inputsChanged(); // 用户编辑后发出

private:
    struct FieldWidget {
        InputField f;
        QWidget* w = nullptr;
        QLabel*  unitLabel = nullptr;
    };
    QVector<FieldWidget> fields_;
    QFormLayout* form_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* note_ = nullptr;

    void clearForm();
    QWidget* makeWidget(const InputField& f, const QVariant& def) const;
    QVariant widgetValue(const FieldWidget& fw) const;
    bool checkRequired(const FieldWidget& fw) const;
};


#endif // EDITORPANEL_H
