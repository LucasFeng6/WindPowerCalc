#ifndef EDITORPANEL_H
#define EDITORPANEL_H

#include "Spec.h"
#include <QWidget>
#include <QMap>

class QFormLayout;
class QLineEdit;
class QLabel;

class EditorPanel : public QWidget {
    Q_OBJECT
public:
    // Switch to given project spec and populate inputs (defaults if empty)
    explicit EditorPanel(QWidget* parent=nullptr);

    // Set current project spec and inputs; sharedInputs used for fields with sharedKey
    void setProject(const ProjectSpec& spec, const QMap<QString,QVariant>& curInputs,
                    const QMap<QString,QVariant>& sharedInputs);

    // Read current form inputs, split into own vs shared; return false if validation fails
    bool collectInputs(QMap<QString,QVariant>& ownOut, QMap<QString,QVariant>& sharedOut, QString* err=nullptr) const;

    // Focus the first editable field (if any)
    void focusFirstField();

signals:
    void inputsChanged(); // 用户编辑后发出

private:
    struct FieldWidget {
        InputField f;
        QWidget* w = nullptr;
    };
    QVector<FieldWidget> fields_;
    QFormLayout* form_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* note_ = nullptr;

    void clearForm();
    QWidget* makeWidget(const InputField& f, const QVariant& def, bool hasValue) const;
    QVariant widgetValue(const FieldWidget& fw) const;
    bool checkRequired(const FieldWidget& fw) const;
    bool focusNextField(QLineEdit* current);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};


#endif // EDITORPANEL_H
