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
}

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent) {
    spec_ = SpecLoader::loadDefault();
    initUi();

    // 初始新建一个方案
    addScheme();
}

void MainWindow::initUi() {
    setWindowTitle(u8"海上风电全生命周期费用计算");

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
    }
    
    // 添加"年运行费"标题行
    auto* annualCostTitle = new QStandardItem(u8"年运行费");
    annualCostTitle->setFont(titleFont);
    annualCostTitle->setForeground(QBrush(QColor(0, 100, 200)));
    annualCostTitle->setFlags(Qt::ItemIsEnabled);
    auto* annualCostSummary = new QStandardItem();
    annualCostSummary->setFlags(Qt::ItemIsEnabled);
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
    for (int i = 0; i < spec_.items.size(); ++i) {
        refreshProjectSummaryRow(i);
    }
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
    
    // 更新此行的摘要
    if (idx.isValid()) {
        QStandardItem* item = projectModel_->itemFromIndex(idx);
        if (item) {
            int rowId = item->data().toInt();
            if (rowId >= 0 && rowId < spec_.items.size()) {
                refreshProjectSummaryRow(rowId);
            }
        }
    }
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

    // Recompute others after shared inputs change
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
    
    // 刷新摘要
    for (int i = 0; i < spec_.items.size(); ++i) {
        refreshProjectSummaryRow(i);
    }
}

void MainWindow::refreshProjectSummaryRow(int row) {
    if (row < 0 || row >= spec_.items.size()) return;
    
    const auto& spec = spec_.items[row];
    Scheme* sch = currentScheme();
    if (!sch) return;
    
    // 找到对应的树节点
    // 遍历所有分组节点
    for (int g = 0; g < projectModel_->rowCount(); ++g) {
        QStandardItem* groupItem = projectModel_->item(g, 0);
        if (!groupItem) continue;
        
        for (int r = 0; r < groupItem->rowCount(); ++r) {
            QStandardItem* nameItem = groupItem->child(r, 0);
            if (!nameItem) continue;
            
            int itemRow = nameItem->data().toInt();
            if (itemRow == row) {
                QStandardItem* summaryItem = groupItem->child(r, 1);
                if (!summaryItem) continue;
                
                if (spec.groupHeader) {
                    summaryItem->setText(u8"（分组标题）");
                } else if (sch->results.contains(spec.id)) {
                    double val = sch->results[spec.id];
                    summaryItem->setText(QString::number(val, 'f', 2) + " " + spec.unit);
                } else if (sch->inputs.contains(spec.id)) {
                    summaryItem->setText(u8"已填写（计算失败）");
                } else {
                    summaryItem->setText(u8"未填写");
                }
                return;
            }
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
    
    QVector<double> initialInvestTotals(schemes_.size(), 0.0);  // 初期投资总计
    QVector<double> annualCostTotals(schemes_.size(), 0.0);     // 年费用总计
    
    // ========== 初期投资部分 ==========
    auto addSectionTitle = [this](const QString& title) {
        QList<QStandardItem*> titleRow;
        auto* titleItem = new QStandardItem(title);
        titleItem->setBackground(QBrush(QColor(180, 200, 255)));
        QFont titleFont = titleItem->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleItem->setFont(titleFont);
        titleRow << titleItem;
        
        auto* unitItem = new QStandardItem("");
        unitItem->setBackground(QBrush(QColor(180, 200, 255)));
        titleRow << unitItem;
        
        for (int i = 0; i < schemes_.size(); ++i) {
            auto* item = new QStandardItem("");
            item->setBackground(QBrush(QColor(180, 200, 255)));
            titleRow << item;
        }
        resultModel_->appendRow(titleRow);
    };
    
    addSectionTitle(u8"初期投资");
    
    for (const auto& g : spec_.groupsInOrder) {
        if (!initialGroups.contains(g)) continue;
        
        // 添加分组标题行
        QList<QStandardItem*> groupRow;
        auto* gName = new QStandardItem(QString(u8"【%1】").arg(g));
        gName->setBackground(QBrush(QColor(220, 220, 220)));
        QFont f = gName->font();
        f.setBold(true);
        gName->setFont(f);
        groupRow << gName;
        
        auto* gUnit = new QStandardItem("");
        gUnit->setBackground(QBrush(QColor(220, 220, 220)));
        groupRow << gUnit;
        
        for (int i = 0; i < schemes_.size(); ++i) {
            auto* item = new QStandardItem("");
            item->setBackground(QBrush(QColor(220, 220, 220)));
            groupRow << item;
        }
        resultModel_->appendRow(groupRow);
        
        // 添加该分组的所有项目
        const auto& rows = spec_.groupRows.value(g);
        for (int idx : rows) {
            if (idx < 0 || idx >= spec_.items.size()) continue;
            const auto& spec = spec_.items[idx];
            
            QList<QStandardItem*> row;
            row << new QStandardItem("  " + spec.label);
            row << new QStandardItem(spec.unit);
            
            for (int i = 0; i < schemes_.size(); ++i) {
                const auto& sch = schemes_[i];
                QString txt;
                double val = 0.0;
                if (sch.results.contains(spec.id)) {
                    val = sch.results[spec.id];
                    txt = QString::number(val, 'f', 2);
                    initialInvestTotals[i] += val;
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
    
    // ========== 年运行费部分 ==========
    addSectionTitle(u8"年运行费");
    
    for (const auto& g : spec_.groupsInOrder) {
        if (!annualGroups.contains(g)) continue;
        
        // 添加分组标题行
        QList<QStandardItem*> groupRow;
        auto* gName = new QStandardItem(QString(u8"【%1】").arg(g));
        gName->setBackground(QBrush(QColor(220, 220, 220)));
        QFont f = gName->font();
        f.setBold(true);
        gName->setFont(f);
        groupRow << gName;
        
        auto* gUnit = new QStandardItem("");
        gUnit->setBackground(QBrush(QColor(220, 220, 220)));
        groupRow << gUnit;
        
        for (int i = 0; i < schemes_.size(); ++i) {
            auto* item = new QStandardItem("");
            item->setBackground(QBrush(QColor(220, 220, 220)));
            groupRow << item;
        }
        resultModel_->appendRow(groupRow);
        
        // 添加该分组的所有项目
        const auto& rows = spec_.groupRows.value(g);
        for (int idx : rows) {
            if (idx < 0 || idx >= spec_.items.size()) continue;
            const auto& spec = spec_.items[idx];
            
            QList<QStandardItem*> row;
            row << new QStandardItem("  " + spec.label);
            row << new QStandardItem(spec.unit);
            
            for (int i = 0; i < schemes_.size(); ++i) {
                const auto& sch = schemes_[i];
                QString txt;
                double val = 0.0;
                if (sch.results.contains(spec.id)) {
                    val = sch.results[spec.id];
                    txt = QString::number(val, 'f', 2);
                    annualCostTotals[i] += val;
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
    
    // ========== 总计行 ==========
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
    
    // 空行
    QList<QStandardItem*> emptyRow;
    for (int i = 0; i < 2 + schemes_.size(); ++i) {
        emptyRow << new QStandardItem("");
    }
    resultModel_->appendRow(emptyRow);
    
    // 初期投资总计
    addTotalRow(u8"初期投资", u8"万元", initialInvestTotals, QColor(255, 255, 200));
    
    // 年费用总计
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
