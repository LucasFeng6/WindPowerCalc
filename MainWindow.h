#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Spec.h"
#include "Calculator.h"
#include <QMainWindow>
#include <QMap>
#include <QVariant>

class QListWidget;
class QTreeView;
class QTableView;
class QStandardItemModel;
class EditorPanel;
class QAction;

struct Scheme {
    QString name;
    QMap<QString, QMap<QString,QVariant>> inputs; // projId -> (field -> value)
    QMap<QString, double> results;                // projId -> result
    QMap<QString, QVariant> sharedInputs;         // sharedKey -> value
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);

private slots:
    void addScheme();
    void duplicateScheme();
    void removeScheme();
    void onSchemeListChanged(int idx);
    void onSchemeItemDoubleClicked();
    void onProjectSelectionChanged();
    void onEditChanged();
    void onGenerate();
    void onSave();
    void onLoad();
    void onExportCsv();

private:
    // UI
    QListWidget* schemesList_ = nullptr;
    QTreeView*  projectView_ = nullptr;
    QStandardItemModel* projectModel_ = nullptr;
    EditorPanel* editor_ = nullptr;
    QTableView* resultView_ = nullptr;
    QStandardItemModel* resultModel_ = nullptr;

    QAction* actGen_ = nullptr;
    QAction* actSave_ = nullptr;
    QAction* actLoad_ = nullptr;
    QAction* actAdd_  = nullptr;
    QAction* actDup_  = nullptr;
    QAction* actDel_  = nullptr;
    QAction* actExport_ = nullptr;

    // Data
    ProjectSpecSet spec_;
    Calculator calc_;
    QVector<Scheme> schemes_;  // 与 list 同步

    // helpers
    void initUi();
    void buildProjectModel();
    int currentSchemeIndex() const;
    Scheme* currentScheme();
    const ProjectSpec* projectSpecFromIndex(const QModelIndex& idx) const;
    void refreshProjectSummaryRow(int row);
    void rebuildResultHeader();
    void rebuildResultBody(); // 使用 schemes_ 的 results
    void setStatusInfo(const QString& msg);
};

#endif // MAINWINDOW_H
