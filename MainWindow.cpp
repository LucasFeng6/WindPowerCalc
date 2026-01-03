#include "MainWindow.h"
#include "SpecLoader.h"
#include "EditorPanel.h"

#include <QListWidget>
#include <QTreeView>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QFile>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QInputDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QStatusBar>
#include <ActiveQt/QAxObject>
#include <QDir>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <cmath>

namespace {
const QStringList& initialInvestGroups() {
    static const QStringList groups = {
        u8"海上变电部分", u8"陆上变电部分",
        u8"线路部分", u8"其他设备", u8"其他费用"
    };
    return groups;
}

const QStringList& annualCostGroups() {
    static const QStringList groups = {
        u8"损耗费用", u8"维护费", u8"停运损失费", u8"海域租赁费"
    };
    return groups;
}

// 计算当前方案中若干投资分组（海上变电、陆上变电、线路、其他设备）的总投资，写入共享输入
static void updateCapexTotals(const ProjectSpecSet& specSet, Scheme* sch) {
    if (!sch) return;
    double offshore = 0.0;
    double onshore = 0.0;
    double line = 0.0;
    double otherEquip = 0.0;

    for (const auto& spec : specSet.items) {
        if (!sch->results.contains(spec.id)) continue;
        const double val = sch->results.value(spec.id);
        if (spec.group == u8"海上变电部分") {
            offshore += val;
        } else if (spec.group == u8"陆上变电部分") {
            onshore += val;
        } else if (spec.group == u8"线路部分") {
            line += val;
        } else if (spec.group == u8"其他设备") {
            otherEquip += val;
        }
    }

    sch->sharedInputs["offshore_capex_total"] = offshore;
    sch->sharedInputs["onshore_capex_total"] = onshore;
    sch->sharedInputs["line_capex_total"] = line;
    sch->sharedInputs["other_equipment_capex_total"] = otherEquip;
    sch->sharedInputs["core_capex_total"] = offshore + onshore + line + otherEquip;
}
}

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent) {
    spec_ = SpecLoader::loadDefault();
    initUi();

    // 初始新建一个方案
    addScheme();
}

void MainWindow::initUi() {
    setWindowTitle(u8"海上风电送出经济比较");

    // 工具栏
    auto* tb = addToolBar(u8"工具");
    actAdd_ = tb->addAction(u8"新增方案");
    actDup_ = tb->addAction(u8"复制方案");
    actDel_ = tb->addAction(u8"删除方案");
    tb->addSeparator();
    actGen_ = tb->addAction(u8"生成汇总");
    actExport_ = tb->addAction(u8"导出Excel");
    tb->addSeparator();
    actSave_ = tb->addAction(u8"保存方案集");
    actLoad_ = tb->addAction(u8"加载方案集");
    tb->addSeparator();
    actEconomic_ = tb->addAction(u8"经济参数");

    connect(actAdd_, &QAction::triggered, this, &MainWindow::addScheme);
    connect(actDup_, &QAction::triggered, this, &MainWindow::duplicateScheme);
    connect(actDel_, &QAction::triggered, this, &MainWindow::removeScheme);
    connect(actGen_, &QAction::triggered, this, &MainWindow::onGenerate);
    connect(actEconomic_, &QAction::triggered, this, &MainWindow::onEditEconomicParams);
    connect(actSave_, &QAction::triggered, this, &MainWindow::onSave);
    connect(actLoad_, &QAction::triggered, this, &MainWindow::onLoad);
    connect(actExport_, &QAction::triggered, this, &MainWindow::onExportExcel);

    // 上半：分三列；下半：结果
    auto* vSplit = new QSplitter(Qt::Vertical, this);
    auto* hSplit = new QSplitter(Qt::Horizontal, vSplit);

    // 左侧：方案列表（纵向）
    auto* leftPanel = new QWidget(hSplit);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    auto* schemeLabel = new QLabel(u8"方案列表", leftPanel);
    schemeLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    schemesList_ = new QListWidget(leftPanel);
    QFont listFont = schemesList_->font();  //
    listFont.setPointSize(listFont.pointSize() + 2); // 放大字体
    schemesList_->setFont(listFont);
    schemesList_->setStyleSheet("QListWidget::item { height: 36px; }"); // 增高每个项
    schemesList_->setSpacing(2);    //
    leftLayout->addWidget(schemeLabel);
    leftLayout->addWidget(schemesList_);
    leftPanel->setLayout(leftLayout);
    
    connect(schemesList_, &QListWidget::currentRowChanged, this, &MainWindow::onSchemeListChanged);
    connect(schemesList_, &QListWidget::itemDoubleClicked, this, &MainWindow::onSchemeItemDoubleClicked);

    projectView_ = new QTreeView(hSplit);
    projectModel_ = new QStandardItemModel(this);
    buildProjectModel();
    projectView_->setModel(projectModel_);
    projectView_->expandAll(); // ensure everything is expanded after the model is bound
    projectView_->header()->setStretchLastSection(true);
    projectView_->setColumnWidth(0, 320); // 第一列：项目
    projectView_->setColumnWidth(1, 100); // 第二列：摘要/状态
    projectView_->setAlternatingRowColors(false);
    projectView_->setRootIsDecorated(true);
    projectView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(projectView_->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::onProjectSelectionChanged);

    editor_ = new EditorPanel(hSplit);
    editor_->setMinimumWidth(400);
    connect(editor_, &EditorPanel::inputsChanged, this, &MainWindow::onEditChanged);

    hSplit->setStretchFactor(0,1); // schemes list
    hSplit->setStretchFactor(1,2); // project list
    hSplit->setStretchFactor(2,2); // editor

    resultView_ = new QTableView(vSplit);
    resultModel_ = new QStandardItemModel(this);
    resultView_->setModel(resultModel_);
    resultView_->horizontalHeader()->setStretchLastSection(false);
    resultView_->verticalHeader()->setVisible(false);
    resultView_->setAlternatingRowColors(true);

    setCentralWidget(vSplit);
    resize(1200, 720);
}

void MainWindow::buildProjectModel() {
    projectModel_->clear();
    projectModel_->setHorizontalHeaderLabels({u8"项目", u8"小计 / 状态"});
    const QBrush groupRowBg(QColor(235, 235, 235));
    const QBrush projectRowBg(Qt::white);
    itemSummaryItems_.clear();
    groupSummaryItems_.clear();
    initialSummaryItem_ = nullptr;
    annualSummaryItem_ = nullptr;

    const auto& initialGroups = initialInvestGroups();
    const auto& annualGroups = annualCostGroups();
    
    // 二级分组（需要缩进显示的）
    QStringList subGroups = {u8"海上变电部分", u8"陆上变电部分"};
    
    // 添加"初期投资"标题行
    auto* initInvestTitle = new QStandardItem(u8"初期投资");
    QFont titleFont = initInvestTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    initInvestTitle->setFont(titleFont);
    initInvestTitle->setForeground(QBrush(QColor(0, 100, 200)));
    initInvestTitle->setFlags(Qt::ItemIsEnabled);
    auto* initInvestSummary = new QStandardItem();
    initInvestSummary->setFlags(Qt::ItemIsEnabled);
    initialSummaryItem_ = initInvestSummary;
    projectModel_->appendRow({initInvestTitle, initInvestSummary});
    
    // 创建分组节点
    QMap<QString, QStandardItem*> groupNodes;
    for (const auto& g : spec_.groupsInOrder) {
        if (initialGroups.contains(g)) {
            auto* gItem0 = new QStandardItem(g);
            gItem0->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            gItem0->setBackground(groupRowBg);
            auto* gItem1 = new QStandardItem();
            gItem1->setBackground(groupRowBg);
            projectModel_->appendRow({gItem0, gItem1});
            groupNodes[g] = gItem0;
            groupSummaryItems_[g] = gItem1;
        }
    }
    
    // 添加初期投资相关的项目
    for (int i = 0; i < spec_.items.size(); ++i) {
        const auto& s = spec_.items[i];
        if (!initialGroups.contains(s.group)) continue;
        
        auto* p = groupNodes.value(s.group, nullptr);
        if (!p) continue;
        
        QString displayName = s.label;
        
        // 如果是二级分组，添加缩进
        if (subGroups.contains(s.group)) {
            displayName = "  " + displayName;
        }
        
        auto* nameIt = new QStandardItem(displayName);
        nameIt->setData(i);
        nameIt->setBackground(projectRowBg);
        auto* summary = new QStandardItem(u8"未填写");
        summary->setBackground(projectRowBg);
        p->appendRow({nameIt, summary});
        itemSummaryItems_[i] = summary;
    }
    
    // 添加"年运行费"标题行
    auto* annualCostTitle = new QStandardItem(u8"年运行费");
    annualCostTitle->setFont(titleFont);
    annualCostTitle->setForeground(QBrush(QColor(0, 100, 200)));
    annualCostTitle->setFlags(Qt::ItemIsEnabled);
    auto* annualCostSummary = new QStandardItem();
    annualCostSummary->setFlags(Qt::ItemIsEnabled);
    annualSummaryItem_ = annualCostSummary;
    projectModel_->appendRow({annualCostTitle, annualCostSummary});
    
    // 创建年运行费分组节点
    for (const auto& g : spec_.groupsInOrder) {
        if (annualGroups.contains(g)) {
            auto* gItem0 = new QStandardItem(g);
            gItem0->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            gItem0->setBackground(groupRowBg);
            auto* gItem1 = new QStandardItem();
            gItem1->setBackground(groupRowBg);
            projectModel_->appendRow({gItem0, gItem1});
            groupNodes[g] = gItem0;
            groupSummaryItems_[g] = gItem1;
        }
    }
    
    // 添加年运行费相关的项目
    for (int i = 0; i < spec_.items.size(); ++i) {
        const auto& s = spec_.items[i];
        if (!annualGroups.contains(s.group)) continue;
        
        auto* p = groupNodes.value(s.group, nullptr);
        if (!p) continue;
        
        QString displayName = s.label;
        
        auto* nameIt = new QStandardItem(displayName);
        nameIt->setData(i);
        nameIt->setBackground(projectRowBg);
        auto* summary = new QStandardItem(u8"未填写");
        summary->setBackground(projectRowBg);
        p->appendRow({nameIt, summary});
        itemSummaryItems_[i] = summary;
    }
}

int MainWindow::currentSchemeIndex() const {
    return schemesList_->currentRow();
}

Scheme* MainWindow::currentScheme() {
    int idx = currentSchemeIndex();
    if (idx<0 || idx>=schemes_.size()) return nullptr;
    return &schemes_[idx];
}

const ProjectSpec* MainWindow::projectSpecFromIndex(const QModelIndex& idx) const {
    if (!idx.isValid()) return nullptr;
    QStandardItem* item = projectModel_->itemFromIndex(idx);
    if (!item) return nullptr;
    QVariant vData = item->data();
    if (!vData.isValid()) return nullptr;
    int rowId = vData.toInt();
    if (rowId < 0 || rowId >= spec_.items.size()) return nullptr;
    return &spec_.items[rowId];
}

void MainWindow::addScheme() {
    static int counter = 1;
    Scheme sch;
    sch.name = QString(u8"方案%1").arg(counter++);
    schemes_.push_back(sch);
    schemesList_->addItem(sch.name);
    schemesList_->setCurrentRow(schemes_.size() - 1);
}

void MainWindow::duplicateScheme() {
    Scheme* cur = currentScheme();
    if (!cur) {
        QMessageBox::information(this, u8"提示", u8"请先选择一个方案");
        return;
    }
    Scheme dup = *cur;
    dup.name += u8"(副本)";
    schemes_.push_back(dup);
    schemesList_->addItem(dup.name);
    schemesList_->setCurrentRow(schemes_.size() - 1);
}

void MainWindow::removeScheme() {
    int idx = currentSchemeIndex();
    if (idx < 0) return;
    if (schemes_.size() == 1) {
        QMessageBox::warning(this, u8"提示", u8"至少保留一个方案");
        return;
    }
    auto ret = QMessageBox::question(this, u8"确认", u8"确定要删除当前方案吗？");
    if (ret != QMessageBox::Yes) return;
    
    schemes_.removeAt(idx);
    delete schemesList_->takeItem(idx);
}

void MainWindow::onSchemeListChanged(int idx) {
    if (idx < 0 || idx >= schemes_.size()) return;
    // 刷新项目树的摘要列
    refreshProjectSummaries();
    // 刷新编辑器
    onProjectSelectionChanged();
}

void MainWindow::onSchemeItemDoubleClicked() {
    int idx = currentSchemeIndex();
    if (idx < 0 || idx >= schemes_.size()) return;
    
    bool ok;
    QString newName = QInputDialog::getText(this, u8"编辑方案名称", 
                                            u8"请输入新的方案名称：", 
                                            QLineEdit::Normal, 
                                            schemes_[idx].name, 
                                            &ok);
    if (ok && !newName.trimmed().isEmpty()) {
        schemes_[idx].name = newName.trimmed();
        schemesList_->item(idx)->setText(newName.trimmed());
    }
}

void MainWindow::onProjectSelectionChanged() {
    const QModelIndex idx = projectView_->currentIndex();
    const ProjectSpec* spec = projectSpecFromIndex(idx);
    if (!spec) {
        editor_->setProject(ProjectSpec(), QMap<QString,QVariant>(), QMap<QString,QVariant>());
        return;
    }
    
    Scheme* sch = currentScheme();
    if (!sch) return;
    
    const auto& inputs = sch->inputs.value(spec->id);
    editor_->setProject(*spec, inputs, sch->sharedInputs);
    editor_->focusFirstField();
    refreshProjectSummaries();
}
void MainWindow::onEditChanged() {
    const QModelIndex idx = projectView_->currentIndex();
    const ProjectSpec* spec = projectSpecFromIndex(idx);
    if (!spec) return;
    
    Scheme* sch = currentScheme();
    if (!sch) return;
    
    QString err;
    QMap<QString, QVariant> inputs;
    QMap<QString, QVariant> sharedInputs;
    if (!editor_->collectInputs(inputs, sharedInputs, &err)) {
        // 有错误，但不阻止用户继续编辑
        return;
    }
    
    // 保存输入
    sch->inputs[spec->id] = inputs;
    for (auto it = sharedInputs.begin(); it != sharedInputs.end(); ++it) {
        sch->sharedInputs[it.key()] = it.value();
    }

    auto mergedInputs = [&](const QString& projId) {
        QMap<QString,QVariant> all = sch->sharedInputs;
        const auto own = sch->inputs.value(projId);
        for (auto it = own.begin(); it != own.end(); ++it) {
            all[it.key()] = it.value();
        }
        return all;
    };

    // 当前项目重新计算
    {
        double result = 0.0;
        const auto allInputs = mergedInputs(spec->id);
        if (calc_.evaluate(*spec, allInputs, result)) {
            sch->results[spec->id] = result;
        } else {
            sch->results.remove(spec->id);
        }
    }

    // 共享输入改变后，重新计算其他已填写项目
    for (const auto& s : spec_.items) {
        if (s.id == spec->id) continue;
        const auto own = sch->inputs.value(s.id);
        if (own.isEmpty()) continue;
        double result = 0.0;
        const auto allInputs = mergedInputs(s.id);
        if (calc_.evaluate(s, allInputs, result)) {
            sch->results[s.id] = result;
        } else {
            sch->results.remove(s.id);
        }
    }

    // 更新变电设备维护费所需的共享输入（分组总投资）
    updateCapexTotals(spec_, sch);

    // 在新的总投资基础上重新计算“变电设备维护费”（若未编辑则使用默认倍率）
    for (const auto& s : spec_.items) {
        if (s.id == "om_substation_equipment") {
            auto own = sch->inputs.value(s.id);
            if (own.isEmpty()) {
                // 若用户从未编辑过该项目，则使用规格里的默认值初始化输入
                QMap<QString,QVariant> defaults;
                for (const auto& f : s.inputs) {
                    if (f.defval.isValid()) {
                        defaults.insert(f.name, f.defval);
                    }
                }
                if (defaults.isEmpty()) {
                    break;
                }
                sch->inputs[s.id] = defaults;
            }
            double result = 0.0;
            const auto allInputs = mergedInputs(s.id);
            if (calc_.evaluate(s, allInputs, result)) {
                sch->results[s.id] = result;
            } else {
                sch->results.remove(s.id);
            }
            break;
        }
    }

    // 在新的总投资基础上重新计算“其他费用”（若未编辑则使用默认倍率）
    for (const auto& s : spec_.items) {
        if (s.id == "other_cost") {
            auto own = sch->inputs.value(s.id);
            if (own.isEmpty()) {
                QMap<QString,QVariant> defaults;
                for (const auto& f : s.inputs) {
                    if (f.defval.isValid()) {
                        defaults.insert(f.name, f.defval);
                    }
                }
                if (defaults.isEmpty()) {
                    break;
                }
                sch->inputs[s.id] = defaults;
            }
            double result = 0.0;
            const auto allInputs = mergedInputs(s.id);
            if (calc_.evaluate(s, allInputs, result)) {
                sch->results[s.id] = result;
            } else {
                sch->results.remove(s.id);
            }
            break;
        }
    }
    
    // 刷新摘要
    refreshProjectSummaries();
}

void MainWindow::refreshProjectSummaries() {
    Scheme* sch = currentScheme();
    const auto& initialGroups = initialInvestGroups();
    const auto& annualGroups = annualCostGroups();
    const QBrush sectionSumFg(QColor(30, 70, 160));
    const QBrush groupSumFg(QColor(150, 90, 0));
    if (!sch) {
        for (auto it = itemSummaryItems_.begin(); it != itemSummaryItems_.end(); ++it) {
            if (it.value()) it.value()->setText(u8"未填写");
        }
        for (auto it = groupSummaryItems_.begin(); it != groupSummaryItems_.end(); ++it) {
            if (it.value()) it.value()->setText(u8"—");
        }
        if (initialSummaryItem_) {
            initialSummaryItem_->setText(u8"—");
            initialSummaryItem_->setData(QVariant(), Qt::ForegroundRole);
        }
        if (annualSummaryItem_) {
            annualSummaryItem_->setText(u8"—");
            annualSummaryItem_->setData(QVariant(), Qt::ForegroundRole);
        }
        return;
    }

    QMap<QString, double> groupTotals;
    QMap<QString, bool> groupHasValues;
    double initialTotal = 0.0;
    double annualTotal = 0.0;
    bool hasInitial = false;
    bool hasAnnual = false;

    for (int i = 0; i < spec_.items.size(); ++i) {
        const auto& spec = spec_.items[i];
        QStandardItem* summaryItem = itemSummaryItems_.value(i, nullptr);
        if (!summaryItem) continue;
        if (spec.groupHeader) {
            summaryItem->setText(u8"（分组标题）");
            continue;
        }

        summaryItem->setData(QVariant(), Qt::ForegroundRole);
        QString text;
        if (sch->results.contains(spec.id)) {
            double val = sch->results.value(spec.id);
            text = QString::number(val, 'f', 2);
            groupTotals[spec.group] += val;
            groupHasValues[spec.group] = true;
            if (initialGroups.contains(spec.group)) {
                initialTotal += val;
                hasInitial = true;
            } else if (annualGroups.contains(spec.group)) {
                annualTotal += val;
                hasAnnual = true;
            }
        } else if (sch->inputs.contains(spec.id)) {
            text = u8"已填写（计算失败）";
        } else {
            text = u8"未填写";
        }
        summaryItem->setText(text);
    }

    for (auto it = groupSummaryItems_.begin(); it != groupSummaryItems_.end(); ++it) {
        const QString groupName = it.key();
        QStandardItem* summaryItem = it.value();
        if (!summaryItem) continue;
        const double total = groupTotals.value(groupName, 0.0);
        const bool hasValue = groupHasValues.value(groupName, false);
        if (hasValue) {
            const QString unit = initialGroups.contains(groupName) ? u8" 万元" : u8" 万元/年";
            summaryItem->setText(QString::number(total, 'f', 0) + unit);
            summaryItem->setForeground(groupSumFg);
        } else {
            summaryItem->setText(u8"—");
            summaryItem->setData(QVariant(), Qt::ForegroundRole);
        }
    }

    if (initialSummaryItem_) {
        if (hasInitial) {
            initialSummaryItem_->setText(QString::number(initialTotal, 'f', 0) + u8" 万元");
            initialSummaryItem_->setForeground(sectionSumFg);
        } else {
            initialSummaryItem_->setText(u8"—");
            initialSummaryItem_->setData(QVariant(), Qt::ForegroundRole);
        }
    }
    if (annualSummaryItem_) {
        if (hasAnnual) {
            annualSummaryItem_->setText(QString::number(annualTotal, 'f', 0) + u8" 万元/年");
            annualSummaryItem_->setForeground(sectionSumFg);
        } else {
            annualSummaryItem_->setText(u8"—");
            annualSummaryItem_->setData(QVariant(), Qt::ForegroundRole);
        }
    }
}

void MainWindow::onGenerate() {
    if (schemes_.isEmpty()) return;
    
    rebuildResultHeader();
    rebuildResultBody();
    hasSummary_ = true;
    
    setStatusInfo(QString(u8"已生成 %1 个方案的汇总").arg(schemes_.size()));
}

void MainWindow::rebuildResultHeader() {
    resultModel_->clear();
    QStringList headers;
    headers << u8"项目" << u8"单位";
    for (const auto& sch : schemes_) {
        headers << sch.name;
    }
    resultModel_->setHorizontalHeaderLabels(headers);
    resultView_->setColumnWidth(0, 200);
    resultView_->setColumnWidth(1, 100);
    for (int i = 2; i < headers.size(); ++i) {
        resultView_->setColumnWidth(i, 150);
    }
}


void MainWindow::rebuildResultBody() {
    resultModel_->removeRows(0, resultModel_->rowCount());
    
    const auto& initialGroups = initialInvestGroups();
    const auto& annualGroups = annualCostGroups();
    const QBrush sectionTitleBg(QColor(180, 200, 255));
    const QBrush groupRowBg(QColor(220, 220, 220));
    const QBrush sectionSumFg(QColor(30, 70, 160));
    const QBrush groupSumFg(QColor(150, 90, 0));
    
    QVector<double> initialInvestTotals(schemes_.size(), 0.0);
    QVector<double> annualCostTotals(schemes_.size(), 0.0);
    
    auto addSectionTitle = [this, sectionTitleBg](const QString& title) {
        QList<QStandardItem*> titleRow;
        auto* titleItem = new QStandardItem(title);
        titleItem->setBackground(sectionTitleBg);
        QFont titleFont = titleItem->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleItem->setFont(titleFont);
        titleRow << titleItem;
        
        auto* unitItem = new QStandardItem("");
        unitItem->setBackground(sectionTitleBg);
        titleRow << unitItem;
        
        QVector<QStandardItem*> dataCells;
        for (int i = 0; i < schemes_.size(); ++i) {
            auto* item = new QStandardItem("");
            item->setBackground(sectionTitleBg);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            titleRow << item;
            dataCells << item;
        }
        resultModel_->appendRow(titleRow);
        return dataCells;
    };
    
    auto addGroupRow = [this, groupRowBg, groupSumFg](const QString& groupName, const QString& unit,
                                                      const QVector<double>& totals,
                                                      const QVector<bool>& hasValues) {
        QList<QStandardItem*> groupRow;
        auto* gName = new QStandardItem(QString(u8"【%1】").arg(groupName));
        gName->setBackground(groupRowBg);
        QFont f = gName->font();
        f.setBold(true);
        gName->setFont(f);
        groupRow << gName;
        
        auto* gUnit = new QStandardItem(unit);
        gUnit->setBackground(groupRowBg);
        groupRow << gUnit;
        
        for (int i = 0; i < schemes_.size(); ++i) {
            const bool hasValue = (i < hasValues.size()) ? hasValues[i] : false;
            QString txt = hasValue ? QString::number(totals[i], 'f', 0) : "-";
            auto* item = new QStandardItem(txt);
            item->setBackground(groupRowBg);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (hasValue) {
                item->setForeground(groupSumFg);
            } else {
                item->setData(QVariant(), Qt::ForegroundRole);
            }
            groupRow << item;
        }
        resultModel_->appendRow(groupRow);
    };
    
    auto updateSectionTotals = [&](const QVector<double>& totals,
                                   const QVector<QStandardItem*>& cells) {
        for (int i = 0; i < cells.size() && i < totals.size(); ++i) {
            auto* item = cells[i];
            if (!item) continue;
            item->setText(QString::number(totals[i], 'f', 0));
            item->setForeground(sectionSumFg);
        }
    };
    
    const auto initialTitleCells = addSectionTitle(u8"初期投资");
    
    for (const auto& g : spec_.groupsInOrder) {
        if (!initialGroups.contains(g)) continue;
        
        const auto& rows = spec_.groupRows.value(g);
        QVector<double> groupTotals(schemes_.size(), 0.0);
        QVector<bool> groupHasValues(schemes_.size(), false);
        for (int idx : rows) {
            if (idx < 0 || idx >= spec_.items.size()) continue;
            const auto& spec = spec_.items[idx];
            for (int i = 0; i < schemes_.size(); ++i) {
                const auto& sch = schemes_[i];
                if (sch.results.contains(spec.id)) {
                    groupTotals[i] += sch.results.value(spec.id);
                    groupHasValues[i] = true;
                }
            }
        }
        addGroupRow(g, u8"万元", groupTotals, groupHasValues);
        for (int i = 0; i < schemes_.size(); ++i) {
            if (groupHasValues[i]) initialInvestTotals[i] += groupTotals[i];
        }
        
        for (int idx : rows) {
            if (idx < 0 || idx >= spec_.items.size()) continue;
            const auto& spec = spec_.items[idx];
            
            QList<QStandardItem*> row;
            row << new QStandardItem("  " + spec.label);
            row << new QStandardItem("");
            
            for (int i = 0; i < schemes_.size(); ++i) {
                const auto& sch = schemes_[i];
                QString txt;
                if (sch.results.contains(spec.id)) {
                    const double val = sch.results.value(spec.id);
                    txt = QString::number(val, 'f', 2);
                } else {
                    txt = "-";
                }
                auto* item = new QStandardItem(txt);
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                row << item;
            }
            resultModel_->appendRow(row);
        }
    }
    
    updateSectionTotals(initialInvestTotals, initialTitleCells);
    
    const auto annualTitleCells = addSectionTitle(u8"年运行费");
    
    for (const auto& g : spec_.groupsInOrder) {
        if (!annualGroups.contains(g)) continue;
        
        const auto& rows = spec_.groupRows.value(g);
        QVector<double> groupTotals(schemes_.size(), 0.0);
        QVector<bool> groupHasValues(schemes_.size(), false);
        for (int idx : rows) {
            if (idx < 0 || idx >= spec_.items.size()) continue;
            const auto& spec = spec_.items[idx];
            for (int i = 0; i < schemes_.size(); ++i) {
                const auto& sch = schemes_[i];
                if (sch.results.contains(spec.id)) {
                    groupTotals[i] += sch.results.value(spec.id);
                    groupHasValues[i] = true;
                }
            }
        }
        addGroupRow(g, u8"万元/年", groupTotals, groupHasValues);
        for (int i = 0; i < schemes_.size(); ++i) {
            if (groupHasValues[i]) annualCostTotals[i] += groupTotals[i];
        }
        
        for (int idx : rows) {
            if (idx < 0 || idx >= spec_.items.size()) continue;
            const auto& spec = spec_.items[idx];
            
            QList<QStandardItem*> row;
            row << new QStandardItem("  " + spec.label);
            row << new QStandardItem("");
            
            for (int i = 0; i < schemes_.size(); ++i) {
                const auto& sch = schemes_[i];
                QString txt;
                if (sch.results.contains(spec.id)) {
                    const double val = sch.results.value(spec.id);
                    txt = QString::number(val, 'f', 2);
                } else {
                    txt = "-";
                }
                auto* item = new QStandardItem(txt);
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                row << item;
            }
            resultModel_->appendRow(row);
        }
    }
    
    updateSectionTotals(annualCostTotals, annualTitleCells);
    
    QVector<double> annualFeeTotals(schemes_.size(), 0.0);
    const double powTerm = std::pow(1.0 + recoveryRate_, serviceYears_);
    const double denominator = powTerm - 1.0;
    double annuityFactor = 0.0;
    if (std::abs(denominator) > 1e-9) {
        annuityFactor = (recoveryRate_ * powTerm) / denominator;
    }
    for (int i = 0; i < schemes_.size(); ++i) {
        annualFeeTotals[i] = initialInvestTotals[i] * annuityFactor + annualCostTotals[i];
    }
    
    auto addTotalRow = [this](const QString& label, const QString& unit,
                              const QVector<double>& totals, const QColor& bgColor) {
        QList<QStandardItem*> totalRow;
        auto* nameItem = new QStandardItem(label);
        QFont boldFont = nameItem->font();
        boldFont.setBold(true);
        nameItem->setFont(boldFont);
        nameItem->setBackground(QBrush(bgColor));
        totalRow << nameItem;
        
        auto* unitItem = new QStandardItem(unit);
        unitItem->setBackground(QBrush(bgColor));
        totalRow << unitItem;
        
        for (int i = 0; i < schemes_.size(); ++i) {
            auto* item = new QStandardItem(QString::number(totals[i], 'f', 2));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            item->setBackground(QBrush(bgColor));
            item->setFont(boldFont);
            totalRow << item;
        }
        resultModel_->appendRow(totalRow);
    };
    
    QList<QStandardItem*> emptyRow;
    for (int i = 0; i < 2 + schemes_.size(); ++i) {
        emptyRow << new QStandardItem("");
    }
    resultModel_->appendRow(emptyRow);
    
    addTotalRow(u8"初期投资", u8"万元", initialInvestTotals, QColor(255, 255, 200));
    addTotalRow(u8"年费用", u8"万元/年", annualFeeTotals, QColor(255, 220, 200));
}


void MainWindow::setStatusInfo(const QString& msg, int timeoutMs) {
    statusBar()->showMessage(msg, timeoutMs);
}

void MainWindow::onSave() {
    QString path = QFileDialog::getSaveFileName(this, u8"保存方案集", "", "JSON (*.json)");
    if (path.isEmpty()) return;
    
    QJsonObject root;
    root["recoveryRate"] = recoveryRate_;
    root["serviceYears"] = serviceYears_;
    QJsonArray schemesArr;
    
    for (const auto& sch : schemes_) {
        QJsonObject schObj;
        schObj["name"] = sch.name;
        
        QJsonObject inputsObj;
        for (auto it = sch.inputs.begin(); it != sch.inputs.end(); ++it) {
            QJsonObject projInputs;
            for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
                projInputs[it2.key()] = QJsonValue::fromVariant(it2.value());
            }
            inputsObj[it.key()] = projInputs;
        }
        schObj["inputs"] = inputsObj;
        
        QJsonObject resultsObj;
        for (auto it = sch.results.begin(); it != sch.results.end(); ++it) {
            resultsObj[it.key()] = it.value();
        }
        schObj["results"] = resultsObj;

        QJsonObject sharedObj;
        for (auto it = sch.sharedInputs.begin(); it != sch.sharedInputs.end(); ++it) {
            sharedObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        schObj["sharedInputs"] = sharedObj;
        
        schemesArr.append(schObj);
    }
    
    root["schemes"] = schemesArr;
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, u8"错误", u8"无法写入文件");
        return;
    }
    
    file.write(QJsonDocument(root).toJson());
    file.close();
    
    setStatusInfo(u8"已保存方案集");
}

void MainWindow::onLoad() {
    QString path = QFileDialog::getOpenFileName(this, u8"加载方案集", "", "JSON (*.json)");
    if (path.isEmpty()) return;
    
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, u8"错误", u8"无法读取文件");
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        QMessageBox::warning(this, u8"错误", u8"文件格式错误");
        return;
    }
    
    QJsonObject root = doc.object();
    hasSummary_ = false;
    recoveryRate_ = root.value("recoveryRate").toDouble(0.08);
    serviceYears_ = root.value("serviceYears").toDouble(25.0);
    QJsonArray schemesArr = root.value("schemes").toArray();
    
    // 清空现有方案
    schemes_.clear();
    schemesList_->clear();
    
    for (const auto& v : schemesArr) {
        QJsonObject schObj = v.toObject();
        Scheme sch;
        sch.name = schObj.value("name").toString();
        
        QJsonObject inputsObj = schObj.value("inputs").toObject();
        for (auto it = inputsObj.begin(); it != inputsObj.end(); ++it) {
            QJsonObject projInputs = it.value().toObject();
            QMap<QString, QVariant> inputs;
            for (auto it2 = projInputs.begin(); it2 != projInputs.end(); ++it2) {
                inputs[it2.key()] = it2.value().toVariant();
            }
            sch.inputs[it.key()] = inputs;
        }
        
        QJsonObject sharedObj = schObj.value("sharedInputs").toObject();
        for (auto it = sharedObj.begin(); it != sharedObj.end(); ++it) {
            sch.sharedInputs[it.key()] = it.value().toVariant();
        }
        
        QJsonObject resultsObj = schObj.value("results").toObject();
        for (auto it = resultsObj.begin(); it != resultsObj.end(); ++it) {
            sch.results[it.key()] = it.value().toDouble();
        }
        
        schemes_.push_back(sch);
        schemesList_->addItem(sch.name);
    }
    
    if (schemes_.isEmpty()) {
        addScheme();
    } else {
        schemesList_->setCurrentRow(0);
    }
    
    setStatusInfo(QString(u8"已加载 %1 个方案").arg(schemes_.size()));
}

void MainWindow::onExportExcel() {
    if (schemes_.isEmpty()) {   //
        QMessageBox::information(this, u8"提示", u8"请先创建至少一个方案");
        return;
    }

    onGenerate();

    QString path = QFileDialog::getSaveFileName(this, u8"导出Excel", "", "Excel (*.xlsx)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".xlsx", Qt::CaseInsensitive)) {
        path += ".xlsx";
    }

    setStatusInfo(u8"导出中...", 0);
    QApplication::processEvents();

    QAxObject excel("Excel.Application");
    if (excel.isNull()) {
        QMessageBox::warning(this, u8"导出失败", u8"无法启动 Excel，请确认已经安装");
        return;
    }

    excel.setProperty("Visible", false);
    excel.setProperty("DisplayAlerts", false);

    QAxObject* workbooks = nullptr;
    QAxObject* workbook = nullptr;
    QAxObject* sheet = nullptr;

    auto cleanup = [&]() {
        if (workbook) {
            workbook->dynamicCall("Close(bool)", false);
        }
        if (!excel.isNull()) {
            excel.dynamicCall("Quit()");
        }
        delete sheet;
        delete workbook;
        delete workbooks;
        sheet = nullptr;
        workbook = nullptr;
        workbooks = nullptr;
    };

    workbooks = excel.querySubObject("Workbooks");
    if (!workbooks) {
        QMessageBox::warning(this, u8"导出失败", u8"无法创建 Excel 工作簿");
        cleanup();
        return;
    }

    workbooks->dynamicCall("Add()");
    workbook = excel.querySubObject("ActiveWorkbook");
    if (!workbook) {
        QMessageBox::warning(this, u8"导出失败", u8"无法创建 Excel 工作簿");
        cleanup();
        return;
    }

    sheet = workbook->querySubObject("Worksheets(int)", 1);
    if (!sheet) {
        QMessageBox::warning(this, u8"导出失败", u8"无法创建 Excel 工作表");
        cleanup();
        setStatusInfo(u8"导出失败");
        return;
    }

    const int rowCount = resultModel_->rowCount();
    const int columnCount = resultModel_->columnCount();

    auto writeCell = [&](int row, int col, const QVariant& value, bool bold = false) {
        if (QAxObject* cell = sheet->querySubObject("Cells(int,int)", row, col)) {
            cell->setProperty("Value", value);
            if (bold) {
                if (QAxObject* font = cell->querySubObject("Font")) {
                    font->setProperty("Bold", true);
                    delete font;
                }
            }
            delete cell;
        }
    };

    for (int c = 0; c < columnCount; ++c) {
        writeCell(1, c + 1, resultModel_->headerData(c, Qt::Horizontal).toString(), true);
    }

    for (int r = 0; r < rowCount; ++r) {
        for (int c = 0; c < columnCount; ++c) {
            const QModelIndex idx = resultModel_->index(r, c);
            writeCell(r + 2, c + 1, resultModel_->data(idx).toString());
        }
    }

    if (QAxObject* columns = sheet->querySubObject("Columns")) {
        columns->dynamicCall("AutoFit()");
        delete columns;
    }

    workbook->dynamicCall("SaveAs(const QString&)", QDir::toNativeSeparators(path));
    cleanup();

    setStatusInfo(u8"已导出Excel");
    QMessageBox::information(this, u8"导出成功", u8"成功导出到"+path);
}

void MainWindow::onEditEconomicParams() {
    QDialog dlg(this);
    dlg.setWindowTitle(u8"经济参数");

    auto* form = new QFormLayout(&dlg);

    auto* rateSpin = new QDoubleSpinBox(&dlg);
    rateSpin->setRange(0.0, 100.0);
    rateSpin->setDecimals(1);
    rateSpin->setSingleStep(0.5);
    rateSpin->setValue(recoveryRate_ * 100.0);
    form->addRow(u8"投资回报率（%）", rateSpin);

    auto* yearsSpin = new QDoubleSpinBox(&dlg);
    yearsSpin->setRange(1, 100);
    yearsSpin->setDecimals(0);
    yearsSpin->setSingleStep(1);
    yearsSpin->setValue(serviceYears_);
    form->addRow(u8"使用年限（年）", yearsSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        recoveryRate_ = rateSpin->value() / 100.0;
        serviceYears_ = yearsSpin->value();
        if (hasSummary_) {
            rebuildResultBody();
        }
    }
}
