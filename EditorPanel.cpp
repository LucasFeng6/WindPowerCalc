#include "EditorPanel.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QDoubleValidator>
#include <QSizePolicy>
#include <QFont>
#include <QVariant>
#include <QtGlobal>
#include <QKeyEvent>
#include <limits>
#include <algorithm>
#include <cmath>

EditorPanel::EditorPanel(QWidget* parent): QWidget(parent) {
    auto* lay = new QVBoxLayout(this);

    QFont baseFont = font();
    baseFont.setPointSize(baseFont.pointSize() + 2);
    setFont(baseFont);

    title_ = new QLabel(u8"<b>请选择左侧项目</b>", this);
    QFont titleFont = baseFont;
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title_->setFont(titleFont);

    note_  = new QLabel("", this);
    note_->setWordWrap(true);
    note_->setFont(baseFont);

    form_ = new QFormLayout();
    form_->setSpacing(14);
    form_->setLabelAlignment(Qt::AlignVCenter);
    form_->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
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

QWidget* EditorPanel::makeWidget(const InputField& f, const QVariant& def, bool hasValue) const {
    auto* edit = new QLineEdit;
    edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    edit->setFixedHeight(32);
    edit->setMinimumWidth(120);
    edit->setMaximumWidth(220);
    edit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* validator = new QDoubleValidator(edit);
    const double minVal = std::max(0.0, f.min);
    double maxVal = std::numeric_limits<double>::max();
    if (std::isfinite(f.max) && f.max >= 0.0) {
        maxVal = std::max(f.max, minVal);
    }
    validator->setRange(minVal, maxVal);
    validator->setNotation(QDoubleValidator::StandardNotation);
    validator->setDecimals(12);
    edit->setValidator(validator);

    if (hasValue && def.isValid()) {
        edit->setText(def.toString());
    } else if (!hasValue && f.defval.isValid()
               && f.defval.canConvert<double>()
               && std::abs(f.defval.toDouble()) > 1e-12) {
        // 无保存值，且默认值非0，显示默认值
        edit->setText(f.defval.toString());
    }

    edit->installEventFilter(const_cast<EditorPanel*>(this));
    return edit;
}

void EditorPanel::setProject(const ProjectSpec& spec, const QMap<QString,QVariant>& curInputs,
                             const QMap<QString,QVariant>& sharedInputs) {
    clearForm();
    if (!spec.label.isEmpty()) {
        title_->setText(QString(u8"<b>%1</b>（%2）").arg(spec.label, spec.unit));
    } else {
        title_->setText(u8"<b>请选择左侧项目</b>");
    }
    note_->setText(spec.note);

    for (const auto& f : spec.inputs) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0,0,0,0);
        const bool hasValue = f.sharedKey.isEmpty()
                                  ? curInputs.contains(f.name)
                                  : sharedInputs.contains(f.sharedKey);
        const QVariant def = f.sharedKey.isEmpty()
                             ? curInputs.value(f.name, f.defval)
                             : sharedInputs.value(f.sharedKey, f.defval);
        QWidget* w = makeWidget(f, def, hasValue);
        auto* unitLabel = new QLabel(f.unit, row);
        unitLabel->setMinimumWidth(80);
        unitLabel->setStyleSheet("color:gray");
        hl->addWidget(w);
        hl->addWidget(unitLabel);
        row->setLayout(hl);

        auto* lbl = new QLabel(f.required ? (f.label + " *") : f.label, this);
        lbl->setMinimumHeight(32);
        form_->addRow(lbl, row);

        FieldWidget fw; fw.f = f; fw.w = w;
        fields_.push_back(fw);

        if (auto* edit = qobject_cast<QLineEdit*>(w))
            connect(edit, &QLineEdit::textChanged, this, &EditorPanel::inputsChanged);
    }
}

QVariant EditorPanel::widgetValue(const FieldWidget& fw) const {
    if (auto* edit = qobject_cast<QLineEdit*>(fw.w)) return edit->text();
    return {};
}

bool EditorPanel::checkRequired(const FieldWidget& fw) const {
    if (!fw.f.required) return true;
    const QVariant v = widgetValue(fw);
    return !v.toString().trimmed().isEmpty();
}

bool EditorPanel::collectInputs(QMap<QString,QVariant>& ownOut, QMap<QString,QVariant>& sharedOut, QString* err) const {
    ownOut.clear();
    sharedOut.clear();
    for (const auto& fw : fields_) {
        if (!checkRequired(fw)) {
            if (err) *err = QString(u8"必填项未填写：%1").arg(fw.f.label);
            return false;
        }
        const QString text = widgetValue(fw).toString().trimmed();
        QVariant v;
        if (text.isEmpty() && fw.f.defval.isValid()) {
            v = fw.f.defval;
        } else {
            v = text;
        }
        if (!fw.f.sharedKey.isEmpty()) {
            sharedOut.insert(fw.f.sharedKey, v);
        } else {
            ownOut.insert(fw.f.name, v);
        }
    }
    return true;
}

void EditorPanel::focusFirstField() {
    for (const auto& fw : fields_) {
        if (auto* edit = qobject_cast<QLineEdit*>(fw.w)) {
            edit->setFocus(Qt::TabFocusReason);
            edit->selectAll();
            break;
        }
    }
}

bool EditorPanel::focusNextField(QLineEdit* current) {
    if (!current) return false;
    for (int i = 0; i < fields_.size(); ++i) {
        if (fields_[i].w == current) {
            for (int j = i + 1; j < fields_.size(); ++j) {
                if (auto* edit = qobject_cast<QLineEdit*>(fields_[j].w)) {
                    edit->setFocus(Qt::TabFocusReason);
                    edit->selectAll();
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

bool EditorPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        if (auto* edit = qobject_cast<QLineEdit*>(obj)) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (focusNextField(edit)) {
                    keyEvent->accept();
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
