#include "EditorPanel.h"
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QFont>
#include <QVariant>
#include <QtGlobal>
#include <limits>

namespace {
class ZeroDefaultDoubleSpinBox : public QDoubleSpinBox {
public:
    explicit ZeroDefaultDoubleSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent) {}

protected:
    QString textFromValue(double value) const override {
        if (qFuzzyIsNull(value)) return QStringLiteral("0");
        return QDoubleSpinBox::textFromValue(value);
    }
};
}

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

QWidget* EditorPanel::makeWidget(const InputField& f, const QVariant& def) const {
    QWidget* w = nullptr;
    if (f.type == "double") {
        auto* sp = new ZeroDefaultDoubleSpinBox;
        sp->setRange(f.min, f.max);
        sp->setDecimals(2);
        sp->setValue(def.isValid() ? def.toDouble() : 0.0);
        sp->setKeyboardTracking(false);
        sp->setFixedHeight(32);
        sp->setMinimumWidth(120);
        sp->setMaximumWidth(220);
        sp->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        w = sp;
    } else if (f.type == "int") {
        auto* sp = new QSpinBox;
        const double dMin = std::numeric_limits<double>::lowest();
        const double dMax = std::numeric_limits<double>::max();
        int minVal = (f.min == dMin) ? 0 : int(f.min);
        int maxVal = (f.max == dMax) ? std::numeric_limits<int>::max() : int(f.max);
        sp->setRange(minVal, maxVal);
        sp->setValue(def.isValid() ? def.toInt() : 0);
        sp->setKeyboardTracking(false);
        sp->setFixedHeight(32);
        sp->setMinimumWidth(120);
        sp->setMaximumWidth(220);
        sp->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        w = sp;
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
        hl->addWidget(w);
        hl->addWidget(unitLabel);
        row->setLayout(hl);
        auto* lbl = new QLabel(f.required ? (f.label + " *") : f.label, this);
        lbl->setMinimumHeight(32);
        form_->addRow(lbl, row);

        FieldWidget fw; fw.f = f; fw.w = w; fw.unitLabel = unitLabel;
        fields_.push_back(fw);

        // 变更即通知
        connect(w, &QWidget::destroyed, this, []{});
        if (auto* sp = qobject_cast<QDoubleSpinBox*>(w)) 
            connect(sp, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &EditorPanel::inputsChanged);
        if (auto* sp = qobject_cast<QSpinBox*>(w))       
            connect(sp, qOverload<int>(&QSpinBox::valueChanged), this, &EditorPanel::inputsChanged);
        
    }
}

QVariant EditorPanel::widgetValue(const FieldWidget& fw) const {
    if (auto* sp = qobject_cast<QDoubleSpinBox*>(fw.w)) return sp->value();
    if (auto* sp = qobject_cast<QSpinBox*>(fw.w))       return sp->value();

    return {};
}

bool EditorPanel::checkRequired(const FieldWidget& fw) const {
    if (!fw.f.required) return true;
    const QVariant v = widgetValue(fw);
    double val = v.toDouble();
    return val != 0.0;
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
