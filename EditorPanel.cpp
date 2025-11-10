#include "EditorPanel.h"
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVariant>

EditorPanel::EditorPanel(QWidget* parent): QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    title_ = new QLabel(u8"<b>请选择左侧项目</b>", this);
    note_  = new QLabel("", this);
    note_->setWordWrap(true);
    form_ = new QFormLayout();
    lay->addWidget(title_);
    lay->addWidget(note_);
    lay->addLayout(form_);
    lay->addStretch();
    setLayout(lay);
}

void EditorPanel::clearForm() {
    while (QLayoutItem* item = form_->takeAt(0)) {
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }
    fields_.clear();
}

QWidget* EditorPanel::makeWidget(const InputField& f, const QVariant& def) const {
    QWidget* w = nullptr;
    if (f.type == "double") {
        auto* sp = new QDoubleSpinBox;
        sp->setRange(f.min, f.max);
        sp->setDecimals(6);
        sp->setValue(def.isValid() ? def.toDouble() : 0.0);
        sp->setKeyboardTracking(false);
        w = sp;
    } else if (f.type == "int") {
        auto* sp = new QSpinBox;
        sp->setRange(int(f.min), int(f.max));
        sp->setValue(def.isValid() ? def.toInt() : 0);
        sp->setKeyboardTracking(false);
        w = sp;
    } else if (f.type == "enum") {
        auto* cb = new QComboBox;
        cb->addItems(f.enumOptions);
        if (def.isValid()) cb->setCurrentText(def.toString());
        w = cb;
    } else if (f.type == "bool") {
        auto* c = new QCheckBox;
        c->setChecked(def.isValid() ? def.toBool() : false);
        w = c;
    } else { // text
        auto* e = new QLineEdit;
        e->setText(def.toString());
        w = e;
    }
    return w;
}

void EditorPanel::setProject(const ProjectSpec& spec, const QMap<QString,QVariant>& curInputs) {
    clearForm();
    title_->setText(QString(u8"<b>%1</b>（结果单位：%2）").arg(spec.label, spec.unit));
    note_->setText(spec.note);

    for (const auto& f : spec.inputs) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0,0,0,0);
        QVariant def = curInputs.value(f.name, f.defval);
        QWidget* w = makeWidget(f, def);
        auto* unitLabel = new QLabel(f.unit, row);
        unitLabel->setMinimumWidth(80);
        unitLabel->setStyleSheet("color:gray");
        hl->addWidget(w, /*stretch*/1);
        hl->addWidget(unitLabel);
        row->setLayout(hl);
        auto* lbl = new QLabel(f.required ? (f.label + " *") : f.label, this);
        form_->addRow(lbl, row);

        FieldWidget fw; fw.f = f; fw.w = w; fw.unitLabel = unitLabel;
        fields_.push_back(fw);

        // 变更即通知
        connect(w, &QWidget::destroyed, this, []{});
        if (auto* sp = qobject_cast<QDoubleSpinBox*>(w)) connect(sp, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &EditorPanel::inputsChanged);
        if (auto* sp = qobject_cast<QSpinBox*>(w))       connect(sp, qOverload<int>(&QSpinBox::valueChanged), this, &EditorPanel::inputsChanged);
        if (auto* e  = qobject_cast<QLineEdit*>(w))      connect(e, &QLineEdit::textChanged, this, &EditorPanel::inputsChanged);
        if (auto* c  = qobject_cast<QCheckBox*>(w))      connect(c, &QCheckBox::toggled, this, &EditorPanel::inputsChanged);
        if (auto* cb = qobject_cast<QComboBox*>(w))      connect(cb, &QComboBox::currentTextChanged, this, &EditorPanel::inputsChanged);
    }
}

QVariant EditorPanel::widgetValue(const FieldWidget& fw) const {
    if (auto* sp = qobject_cast<QDoubleSpinBox*>(fw.w)) return sp->value();
    if (auto* sp = qobject_cast<QSpinBox*>(fw.w))       return sp->value();
    if (auto* cb = qobject_cast<QComboBox*>(fw.w))      return cb->currentText();
    if (auto* c  = qobject_cast<QCheckBox*>(fw.w))      return c->isChecked();
    if (auto* e  = qobject_cast<QLineEdit*>(fw.w))      return e->text();
    return {};
}

bool EditorPanel::checkRequired(const FieldWidget& fw) const {
    if (!fw.f.required) return true;
    const QVariant v = widgetValue(fw);
    if (fw.f.type=="double" || fw.f.type=="int") {
        return true; // 数值0也允许
    }
    return v.isValid() && !v.toString().trimmed().isEmpty();
}

bool EditorPanel::collectInputs(QMap<QString,QVariant>& out, QString* err) const {
    out.clear();
    for (const auto& fw : fields_) {
        if (!checkRequired(fw)) {
            if (err) *err = QString(u8"必填项未填写：%1").arg(fw.f.label);
            return false;
        }
        out.insert(fw.f.name, widgetValue(fw));
    }
    return true;
}
