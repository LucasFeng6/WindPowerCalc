# WindPowerCalc 工程详细文档

## 📋 目录
1. [项目概述](#项目概述)
2. [项目架构](#项目架构)
3. [核心数据结构](#核心数据结构)
4. [UI界面布局](#ui界面布局)
5. [核心类详解](#核心类详解)
6. [业务逻辑流程](#业务逻辑流程)
7. [数据流向图](#数据流向图)
8. [功能模块说明](#功能模块说明)
9. [扩展开发指南](#扩展开发指南)

---

## 项目概述

**WindPowerCalc** 是一个基于 Qt 6 的**风电工程投资计算与方案管理工具**。

### 🎯 核心功能
- **多方案管理**：同时创建、编辑、对比多个投资方案
- **规格驱动**：通过 JSON 配置定义项目规格（输入字段、计算公式）
- **实时计算**：用户输入后立即使用 JavaScript 公式计算结果
- **汇总对比**：生成横向对比表，清晰展示所有方案的结果
- **数据持久化**：支持保存/加载方案集（JSON）、导出 CSV

### 🛠️ 技术栈
- **框架**: Qt 6.8.1 (QtWidgets)
- **语言**: C++17
- **计算引擎**: QJSEngine (JavaScript 表达式求值)
- **数据格式**: JSON (规格定义、方案存储)
- **构建工具**: qmake (.pro 项目文件)

---

## 项目架构

```
WindPowerCalc/
│
├── 数据层 (Data Layer)
│   ├── Spec.h/cpp           - 数据结构定义
│   └── SpecLoader.h/cpp     - 规格加载器
│
├── 业务层 (Business Layer)
│   └── Calculator.h/cpp     - 计算引擎
│
├── UI层 (UI Layer)
│   ├── MainWindow.h/cpp     - 主窗口（核心控制器）
│   └── EditorPanel.h/cpp    - 输入表单编辑器
│
└── 入口 (Entry)
    └── main.cpp             - 程序入口
```

### 模块职责划分

| 模块 | 职责 | 依赖关系 |
|------|------|----------|
| **Spec** | 定义数据结构（项目规格、输入字段） | 无依赖 |
| **SpecLoader** | 从内嵌JSON加载规格定义 | → Spec |
| **Calculator** | 执行公式计算（QJSEngine） | → Spec |
| **EditorPanel** | 动态生成输入表单 | → Spec |
| **MainWindow** | 统筹所有UI和数据流转 | → 所有模块 |

---

## 核心数据结构

### 1. `InputField` - 输入字段定义
```cpp
struct InputField {
    QString name;           // 字段名（用于计算公式中的变量名）
    QString label;          // 显示标签
    QString type;           // 类型：double/int/enum/bool/text
    QString unit;           // 单位（如：km、Ω/km）
    QVariant defval;        // 默认值
    bool required;          // 是否必填
    double min, max;        // 数值范围
    QStringList enumOptions; // 枚举选项
};
```

### 2. `ProjectSpec` - 项目规格
```cpp
struct ProjectSpec {
    QString id;             // 唯一标识（如：subsea_cable_capex）
    QString group;          // 所属分组（如：线路部分）
    QString label;          // 显示名称（如：海缆投资）
    QString unit;           // 结果单位（如：万元/年）
    QVector<InputField> inputs;  // 输入字段列表
    QString formula;        // JavaScript 计算公式
    QString note;           // 备注说明
    bool groupHeader;       // 是否为分组标题行（美化用）
};
```

**示例规格（海缆费用）：**
```json
{
  "id": "subsea_cable",
  "group": "线路部分",
  "label": "海缆费用",
  "unit": "万元",
  "inputs": [
    {"name": "unit_price", "label": "单价", "type": "double", "unit": "万元/km", "required": false, "defval": 0},
    {"name": "length", "label": "长度", "type": "double", "unit": "km", "required": false, "defval": 0}
  ],
  "formula": "result = unit_price * length;"
}
```

### 3. `ProjectSpecSet` - 规格集合
```cpp
struct ProjectSpecSet {
    QVector<ProjectSpec> items;          // 所有项目
    QStringList groupsInOrder;           // 分组展示顺序
    QMap<QString, QVector<int>> groupRows; // 分组→项目索引映射
};
```

### 4. `Scheme` - 方案数据
```cpp
struct Scheme {
    QString name;                                 // 方案名称
    QMap<QString, QMap<QString,QVariant>> inputs; // 输入数据：项目ID → (字段名 → 值)
    QMap<QString, double> results;                // 计算结果：项目ID → 结果值
};
```

**数据关系示例：**
```
Scheme "方案1"
├── inputs
│   ├── "subsea_cable"
│   │   ├── "unit_price" → 150.0
│   │   └── "length" → 50.0
│   └── "offshore_converter"
│       └── "capex" → 8000.0
└── results
    ├── "subsea_cable" → 7500.0  (150 * 50)
    └── "offshore_converter" → 8000.0
```

---

## UI界面布局

### 整体布局结构

```
┌─────────────────────────────────────────────────────────┐
│  工具栏                                                  │
│  [新增方案][复制方案][删除方案] | [生成汇总][导出CSV]... │
├──────────┬──────────────────┬────────────────────────────┤
│          │                  │                            │
│  方案列表 │   项目树视图      │   输入编辑面板             │
│ (ListView)│  (TreeView)      │  (EditorPanel)             │
│          │                  │                            │
│ ┌──────────────┐ │  ├─ 初期投资        │  ┌──────────────────────┐ │
│ │ 方案列表     │ │  │  ├─ 变电部分    │  │ 项目名：海缆费用      │ │
│ │ 方案1        │ │  │  ├─ 海上变电部分│ │ 结果单位：万元        │ │
│ │ 方案2（双击重命名）│ │  │  ├─ 陆上变电部分│ ├──────────────────────┤ │
│ │              │ │  │  ├─ 线路部分    │ │ 单价: [150.0] 万元/km │ │
│ └──────────────┘ │  ├─ 年运行费        │ │ 长度: [50.0]  km      │ │
│                  │  │  ├─ 维护费      │ └──────────────────────┘ │
│                  │  │  ├─ 停运损失费  │                            │
│                  │  │  └─ 海域租赁费  │                            │
├──────────┴──────────────────┴────────────────────────────┤
│  汇总结果表 (TableView)                                  │
│  ┌────────────┬──────┬────────┬────────┐                  │
│  │ 项目       │ 单位 │ 方案1  │ 方案2  │                  │
│  ├────────────┼──────┼────────┼────────┤                  │
│  │初期投资    │      │        │        │ ← 蓝底章节标题    │
│  │【海上变电部分】│  │        │        │ ← 灰底分组标题     │
│  │  海上换流站│ 万元 │ 8000.00│ 7500.00│                  │
│  │…           │ …    │   …    │   …    │                  │
│  │年运行费    │      │        │        │ ← 蓝底章节标题    │
│  │…           │ …    │   …    │   …    │                  │
│  │初期投资    │ 万元 │ 16000.00│ 15000.00│ ← 黄色总计行   │
│  │年费用      │ 万元/年│ 900.00│ 850.00│ ← 橙色总计行      │
│  │全生命周期总投资（占位）│ 万元 │ 16900.00│ 15850.00│ ← 绿色总计行 │
│  └────────────┴──────┴────────┴────────┘                  │
└──────────────────────────────────────────────────────────┘
```

### UI组件层次

```cpp
MainWindow (QMainWindow)
├── ToolBar (QToolBar) - 工具栏
├── CentralWidget: QSplitter (Vertical)
│   ├── QSplitter (Horizontal)
│   │   ├── leftPanel (QWidget)
│   │   │   └── schemesList_ (QListWidget)    [1:2:2 伸缩比]
│   │   ├── projectView_ (QTreeView)
│   │   │   └── projectModel_ (QStandardItemModel)
│   │   └── editor_ (EditorPanel)
│   │       └── QFormLayout (动态生成输入控件)
│   └── resultView_ (QTableView)
│       └── resultModel_ (QStandardItemModel)
└── StatusBar (QStatusBar) - 状态栏
```

---

## 核心类详解

### 📦 1. SpecLoader - 规格加载器

**文件**: `SpecLoader.h/cpp`

**功能**: 从内嵌 JSON 字符串加载项目规格定义

**关键方法**:
```cpp
static ProjectSpecSet loadDefault();  // 加载默认规格
```

**工作流程**:
```
kEmbeddedSpec (JSON字符串)
    ↓ QJsonDocument::fromJson()
解析 groups 数组 → groupsInOrder
    ↓
解析 items 数组 → 每个item调用parseItem()
    ↓ parseItem()
解析 id/group/label/unit/formula/inputs
    ↓
构建 groupRows 索引映射
    ↓
返回 ProjectSpecSet
```

**内嵌规格示例**:
```json
{
  "groups": [
    "变电部分",
    "海上变电部分",
    "陆上变电部分",
    "线路部分",
    "其他设备",
    "其他费用",
    "维护费",
    "停运损失费",
    "海域租赁费"
  ],
  "items": [
    { "id": "offshore_converter", "group": "海上变电部分", "label": "海上换流站", ... },
    { "id": "subsea_cable", "group": "线路部分", "label": "海缆费用", ... },
    { "id": "rent_cable", "group": "海域租赁费", "label": "线路年海域使用费", ... },
    ...
  ]
}
```

---

### 🧮 2. Calculator - 计算引擎

**文件**: `Calculator.h/cpp`

**功能**: 使用 QJSEngine 执行 JavaScript 公式计算

**关键方法**:
```cpp
bool evaluate(
    const ProjectSpec& spec,      // 项目规格
    const InputMap& inputs,        // 输入值
    double& out,                   // 输出结果
    QString* explain = nullptr     // 可选：计算说明
);
```

**计算流程**:
```
1. 检查是否有注册的回调函数
   ├─ 有 → 执行回调
   └─ 无 → 使用 QJSEngine
2. 创建 QJSEngine 实例
3. 将输入值注入全局对象
   例：eng.globalObject().setProperty("unit_price", 150.0)
4. 设置 result=0 初始值
5. 执行 formula 代码
   例：eng.evaluate("result = unit_price * length;")
6. 检查是否有错误
7. 读取 result 值
8. 返回计算结果
```

**公式编写规范**:
- 必须设置 `result` 变量作为输出
- 可以使用任何 JavaScript 语法
- 可以访问所有输入字段（字段名作为变量名）

**示例公式**:
```javascript
// 简单乘法
result = unit_price * length;

// 复杂计算
loss_kWh = (I*I*R_per_km*length*loops/1000.0) * hours;
result = loss_kWh * price / 10000.0;
```

---

### 📝 3. EditorPanel - 输入编辑器

**文件**: `EditorPanel.h/cpp`

**功能**: 根据 `ProjectSpec` 动态生成输入表单

**关键方法**:
```cpp
// 设置当前编辑的项目
void setProject(const ProjectSpec& spec, const QMap<QString,QVariant>& curInputs);

// 收集用户输入
bool collectInputs(QMap<QString,QVariant>& out, QString* err=nullptr);
```

**控件映射规则**:

| 字段类型 | Qt控件 | 说明 |
|---------|--------|------|
| `double` | QDoubleSpinBox | 6位小数精度 |
| `int` | QSpinBox | 整数输入 |
| `enum` | QComboBox | 下拉选择 |
| `bool` | QCheckBox | 勾选框 |
| `text` | QLineEdit | 文本输入 |

**动态生成流程**:
```
setProject() 调用
    ↓
clearForm() - 清空旧表单
    ↓
遍历 spec.inputs
    ↓
为每个 InputField 执行：
    ├─ makeWidget() - 创建对应控件
    ├─ 创建单位标签 (QLabel)
    ├─ 使用 QHBoxLayout 组合
    ├─ 添加到 QFormLayout
    └─ 连接 valueChanged/textChanged 信号 → inputsChanged()
```

**信号机制**:
```cpp
signals:
    void inputsChanged();  // 任何输入变化都会发出此信号
```
连接到 `MainWindow::onEditChanged()` 触发实时计算。

---

### 🏠 4. MainWindow - 主窗口（核心控制器）

**文件**: `MainWindow.h/cpp`

**功能**: 统筹所有UI组件和业务逻辑

#### 关键成员变量

```cpp
// === UI组件 ===
QListWidget* schemesList_;          // 方案列表
QTreeView* projectView_;            // 项目树
QStandardItemModel* projectModel_;  // 项目树数据模型
EditorPanel* editor_;               // 编辑面板
QTableView* resultView_;            // 汇总结果表
QStandardItemModel* resultModel_;   // 结果表数据模型

// === 数据 ===
ProjectSpecSet spec_;               // 项目规格集
Calculator calc_;                   // 计算引擎
QVector<Scheme> schemes_;           // 所有方案（与列表同步）
```

#### 关键槽函数（事件处理）

| 槽函数 | 触发时机 | 功能说明 |
|--------|---------|---------|
| `addScheme()` | 点击"新增方案" | 创建新方案并追加到列表 |
| `duplicateScheme()` | 点击"复制方案" | 复制当前方案 |
| `removeScheme()` | 点击"删除方案" | 删除当前方案（至少保留1个） |
| `onSchemeListChanged(int)` | 切换列表选中项 | 刷新项目树和编辑器 |
| `onSchemeItemDoubleClicked()` | 双击方案列表项 | 弹出输入框修改方案名称 |
| `onProjectSelectionChanged()` | 选择项目树节点 | 加载对应项目到编辑器 |
| `onEditChanged()` | 编辑器输入变化 | 保存输入、计算结果、更新摘要 |
| `onGenerate()` | 点击"生成汇总" | 生成汇总对比表 |
| `onSave()` | 点击"保存方案集" | 导出JSON文件 |
| `onLoad()` | 点击"加载方案集" | 导入JSON文件 |
| `onExportCsv()` | 点击"导出CSV" | 导出CSV文件 |

---

## 业务逻辑流程

### 🔄 1. 程序启动流程

```
main() 入口
    ↓
创建 QApplication
    ↓
创建 MainWindow
    ├─ spec_ = SpecLoader::loadDefault()  ← 加载规格
    ├─ initUi()                            ← 初始化UI
    │   ├─ 创建工具栏
    │   ├─ 创建分割器布局
    │   ├─ buildProjectModel()             ← 构建项目树
    │   └─ 连接信号槽
    └─ addScheme()                         ← 创建初始方案
    ↓
w.show() 显示窗口
    ↓
a.exec() 进入事件循环
```

---

### 📝 2. 编辑输入的完整流程

```
[用户操作] 选择项目树节点
    ↓
onProjectSelectionChanged() 触发
    ├─ 获取 ProjectSpec
    ├─ 获取当前 Scheme 的 inputs
    └─ editor_->setProject(spec, inputs)
        ↓
        EditorPanel::setProject()
            ├─ clearForm()
            ├─ 动态生成输入控件
            └─ 连接 inputsChanged() 信号
    ↓
[用户操作] 修改输入值
    ↓
控件信号 (valueChanged/textChanged) 触发
    ↓
inputsChanged() 信号发出
    ↓
onEditChanged() 触发
    ├─ collectInputs() 收集输入
    ├─ 保存到 scheme->inputs[projId]
    ├─ calc_.evaluate() 计算结果
    ├─ 保存到 scheme->results[projId]
    └─ refreshProjectSummaryRow() 更新摘要
```

**关键点**：
- **实时计算**：每次输入变化都会重新计算
- **数据分离**：输入和结果分别存储
- **UI同步**：计算后立即更新项目树的摘要列

---

### 📊 3. 生成汇总表流程

```
[用户操作] 点击"生成汇总"
    ↓
onGenerate() 触发
    ├─ rebuildResultHeader()      ← 构建表头
    │   └─ ["项目", "单位", "方案1", "方案2", ...]
    └─ rebuildResultBody()        ← 构建表体
        ↓
        初始化累计数组：初期投资 / 年运行费
        ↓
        添加章节标题 "初期投资"
            └─ 遍历初期投资相关分组 → 灰底标题 + 数据行 + 累加初期投资
        添加章节标题 "年运行费"
            └─ 遍历年运行费相关分组 → 灰底标题 + 数据行 + 累加年费用
        追加空行
        输出总计行：
            ├─ 初期投资（黄色）
            ├─ 年费用（橙色）
            └─ 全生命周期总投资（绿色，= 初期 + 年）
```

**表格效果示例**:
```
┌─────────────────────────────┬────────┬────────┬────────┐
│ 项目                        │ 单位   │ 方案1  │ 方案2  │
├─────────────────────────────┼────────┼────────┼────────┤
│初期投资                     │        │        │        │
│【海上变电部分】             │        │        │        │
│  海上换流站                 │ 万元   │ 8000.00│ 7500.00│
│  海上无功补偿平台           │ 万元   │    -   │    -   │
│...                          │ ...    │  ...   │  ...   │
│年运行费                     │        │        │        │
│【维护费】                   │        │        │        │
│  海上变电设备维护费         │ 万元/年│ 120.00 │ 110.00 │
│...                          │ ...    │  ...   │  ...   │
│                             │        │        │        │
│初期投资                     │ 万元   │ 16000.00│15500.00│
│年费用                       │ 万元/年│  900.00│  820.00│
│全生命周期总投资（占位）     │ 万元   │ 16900.00│16320.00│
└─────────────────────────────┴────────┴────────┴────────┘
```

---

### 💾 4. 保存/加载方案流程

#### 保存流程
```
onSave() 触发
    ↓
构建 QJsonObject
    ├─ schemes 数组
    │   ├─ name
    │   ├─ inputs (嵌套对象)
    │   └─ results (对象)
    ↓
QJsonDocument::toJson()
    ↓
写入文件
```

**JSON格式示例**:
```json
{
  "schemes": [
    {
      "name": "方案1",
      "inputs": {
        "subsea_cable": {
          "unit_price": 150,
          "length": 50
        },
        "offshore_converter": {
          "capex": 8000
        }
      },
      "results": {
        "subsea_cable": 7500,
        "offshore_converter": 8000
      }
    }
  ]
}
```

#### 加载流程
```
onLoad() 触发
    ↓
读取文件 → QJsonDocument
    ↓
解析 schemes 数组
    ├─ 遍历每个 schemeObj
    ├─ 解析 name/inputs/results
    ├─ 构建 Scheme 对象
    └─ 添加到 schemes_
    ↓
清空旧 schemesList_
    ↓
为每个 scheme 添加新的列表项
```

---

### 📤 5. 导出CSV流程

```
onExportCsv() 触发
    ↓
创建 QTextStream (UTF-8 + BOM)
    ↓
写表头: "项目,单位,方案1,方案2,..."
    ↓
输出章节 "初期投资"
    └─ 遍历初期投资分组 → 输出 "【分组名称】" + 明细行 + 累加初期投资
输出章节 "年运行费"
    └─ 遍历年运行费用分组 → 输出 "【分组名称】" + 明细行 + 累加年费用
    ↓
写总计行:
    ├─ "初期投资,万元,..."
    ├─ "年费用,万元/年,..."
    └─ "全生命周期总投资（占位）,万元,..."
```

---

## 数据流向图

```
┌─────────────────────────────────────────────────────────────┐
│                     MainWindow (核心控制器)                  │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────┐      ┌─────────────┐      ┌──────────────┐   │
│  │ Scheme 1 │      │ Scheme 2    │      │ Scheme N     │   │
│  ├──────────┤      ├─────────────┤      ├──────────────┤   │
│  │ inputs   │      │ inputs      │      │ inputs       │   │
│  │ results  │      │ results     │      │ results      │   │
│  └──────────┘      └─────────────┘      └──────────────┘   │
│       ↑                   ↑                     ↑            │
│       └───────────────────┴─────────────────────┘            │
│                           │                                  │
│            ┌──────────────┼──────────────┐                  │
│            ↓              ↓              ↓                  │
│   ┌─────────────┐ ┌──────────────┐  ┌──────────────┐      │
│   │schemesList_ │ │ projectView  │  │ EditorPanel  │      │
│   │  (选择方案) │ │  (选择项目)  │  │  (编辑输入)  │      │
│   └─────────────┘ └──────────────┘  └──────────────┘      │
│                           │                   │              │
│                           ↓                   ↓              │
│                    ProjectSpec ←────────→ Calculator        │
│                    (规格定义)            (计算引擎)         │
│                           │                   │              │
│                           ↓                   ↓              │
│                    ┌─────────────────────────┐              │
│                    │   resultView (汇总表)   │              │
│                    └─────────────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

**数据流转说明**:
1. **选择方案** → 切换当前操作的 `Scheme` 对象
2. **选择项目** → 从 `ProjectSpec` 加载规格，从 `Scheme.inputs` 加载已有输入
3. **编辑输入** → 保存到 `Scheme.inputs`，调用 `Calculator` 计算，保存到 `Scheme.results`
4. **生成汇总** → 从所有 `Scheme.results` 读取数据，填充 `resultView`

---

## 功能模块说明

### 📁 1. 项目树视图 (projectView_)

**数据结构**:
```
QStandardItemModel
├─ 初期投资 (章节标题，不可选)
│  ├─ 变电部分 (分组)
│  │   ├─ 海上换流站 [行号=0] | 摘要: 8000.00 万元
│  │   ├─ 海上无功补偿平台 [行号=1] | 摘要: 未填写
│  │   └─ ...
│  ├─ 海上变电部分 (分组，带缩进)
│  │   ├─ ...
│  └─ 线路部分 (分组)
│      └─ 海缆费用 [行号=6] | 摘要: 7500.00 万元
├─ 年运行费 (章节标题，不可选)
│  ├─ 维护费 (分组)
│  │   ├─ 海上变电设备维护费 [行号=8] | 摘要: 120.00 万元/年
│  │   └─ ...
│  ├─ 停运损失费 (分组)
│  └─ 海域租赁费 (分组)
```

**关键逻辑**:
- 每个项目节点存储 `行号` (data)
- 通过行号可反查 `spec_.items[row]`
- 摘要列动态显示：未填写 / 已填写（计算失败） / 结果值

**构建方法**: `buildProjectModel()`
```cpp
// 1. 添加章节标题（初期投资 / 年运行费），仅用于展示
auto* initInvestTitle = new QStandardItem("初期投资");
initInvestTitle->setFlags(Qt::ItemIsEnabled);
projectModel_->appendRow({initInvestTitle, new QStandardItem()});

// 2. 为每个章节指定需要呈现的分组，并创建可选择的分组节点
QStringList initialInvestGroups = {"变电部分", "海上变电部分", ...};
QStringList subGroups = {"海上变电部分", "陆上变电部分"};
for (const auto& g : spec_.groupsInOrder) {
    if (!initialInvestGroups.contains(g)) continue;
    auto* groupItem = new QStandardItem(g);
    projectModel_->appendRow({groupItem, new QStandardItem()});
    groupNodes[g] = groupItem;
}

// 3. 遍历 spec_.items，将同组项目插入对应分组节点
for (int i = 0; i < spec_.items.size(); ++i) {
    const auto& spec = spec_.items[i];
    if (!groupNodes.contains(spec.group)) continue;
    QString display = spec.label;
    if (subGroups.contains(spec.group)) {
        display = "  " + display;  // 二级分组增加缩进
    }
    auto* nameItem = new QStandardItem(display);
    nameItem->setData(i);  // 存储行号
    groupNodes[spec.group]->appendRow({nameItem, new QStandardItem("未填写")});
}
```

---

### 🖊️ 2. 编辑面板 (editor_)

**布局**:
```
┌─────────────────────────────────────┐
│ 标题: 海缆投资（结果单位：万元）    │
├─────────────────────────────────────┤
│ 备注: ... （备注文字换行显示）      │
├─────────────────────────────────────┤
│ 单价:  [150.0] 万元/km              │
│ 长度:  [50.0]  km                   │
└─────────────────────────────────────┘
```

**表单生成**:
```cpp
for (const auto& f : spec.inputs) {
    QWidget* w = makeWidget(f, defaultValue);  // 创建控件
    QLabel* unit = new QLabel(f.unit);         // 单位标签
    // 组合到 QHBoxLayout
    QHBoxLayout* hl = new QHBoxLayout;
    hl->addWidget(w, 1);
    hl->addWidget(unit);
    // 添加到表单
    form_->addRow(f.label + (f.required ? " *" : ""), row);
}
```

**校验机制**: `collectInputs()`
- 必填项检查 (`required`)
- 数值项直接通过（允许0）
- 文本项检查是否为空

---

### 📈 3. 汇总结果表 (resultView_)

**表格构成**:
```
┌─────────────────────────────┬────────┬────────┬────────┐
│ 项目                        │ 单位   │ 方案1  │ 方案2  │  ← 表头
├─────────────────────────────┼────────┼────────┼────────┤
│初期投资                     │        │        │        │  ← 蓝底章节标题
│【海上变电部分】             │        │        │        │  ← 灰底分组标题
│  海上换流站                 │ 万元   │ 8000.00│ 7500.00│  ← 数据行
│  海上无功补偿平台           │ 万元   │    -   │    -   │
│...                          │ ...    │  ...   │  ...   │
│年运行费                     │        │        │        │  ← 蓝底章节标题
│【维护费】                   │        │        │        │
│  海上变电设备维护费         │ 万元/年│ 120.00 │ 110.00 │
│...                          │ ...    │  ...   │  ...   │
│                             │        │        │        │  ← 空行
│初期投资                     │ 万元   │ 16000.00│15500.00│ ← 黄色总计
│年费用                       │ 万元/年│  900.00│  820.00│ ← 橙色总计
│全生命周期总投资（占位）     │ 万元   │ 16900.00│16320.00│ ← 绿色总计
└─────────────────────────────┴────────┴────────┴────────┘
```

**渲染逻辑** (`rebuildResultBody`):
```cpp
QStringList initialInvestGroups = {"变电部分", "海上变电部分", ...};
QStringList annualCostGroups    = {"维护费", "停运损失费", "海域租赁费"};

QVector<double> initialTotals(schemes_.size(), 0.0);
QVector<double> annualTotals(schemes_.size(), 0.0);

auto addSectionTitle = [&](const QString& title) { /* 蓝底行 */ };
auto addGroupHeader  = [&](const QString& group) { /* 灰底行 */ };
auto addTotalRow     = [&](const QString& label,
                           const QString& unit,
                           const QVector<double>& totals,
                           const QColor& color) { /* 彩色总计行 */ };

addSectionTitle("初期投资");
for (const auto& group : spec_.groupsInOrder) {
    if (!initialInvestGroups.contains(group)) continue;
    addGroupHeader(group);
    for (int idx : spec_.groupRows.value(group)) {
        const auto& spec = spec_.items[idx];
        QList<QStandardItem*> row;
        row << new QStandardItem("  " + spec.label);
        row << new QStandardItem(spec.unit);
        for (int i = 0; i < schemes_.size(); ++i) {
            auto* item = new QStandardItem("-");
            if (schemes_[i].results.contains(spec.id)) {
                double val = schemes_[i].results[spec.id];
                item->setText(QString::number(val, 'f', 2));
                initialTotals[i] += val;
            }
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            row << item;
        }
        resultModel_->appendRow(row);
    }
}

addSectionTitle("年运行费");
for (const auto& group : spec_.groupsInOrder) {
    if (!annualCostGroups.contains(group)) continue;
    addGroupHeader(group);
    for (int idx : spec_.groupRows.value(group)) {
        const auto& spec = spec_.items[idx];
        QList<QStandardItem*> row;
        row << new QStandardItem("  " + spec.label);
        row << new QStandardItem(spec.unit);
        for (int i = 0; i < schemes_.size(); ++i) {
            auto* item = new QStandardItem("-");
            if (schemes_[i].results.contains(spec.id)) {
                double val = schemes_[i].results[spec.id];
                item->setText(QString::number(val, 'f', 2));
                annualTotals[i] += val;
            }
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            row << item;
        }
        resultModel_->appendRow(row);
    }
}

resultModel_->appendRow(makeEmptyRow());
addTotalRow("初期投资", "万元", initialTotals, QColor(255,255,200));
addTotalRow("年费用", "万元/年", annualTotals, QColor(255,220,200));

QVector<double> lifeCycleTotals(schemes_.size());
for (int i = 0; i < schemes_.size(); ++i) {
    lifeCycleTotals[i] = initialTotals[i] + annualTotals[i];
}
addTotalRow("全生命周期总投资（占位）", "万元", lifeCycleTotals, QColor(200,255,200));
```

---

### 🔧 4. 方案管理

#### 新增方案 (`addScheme()`)
```cpp
static int counter = 1;  // 方案编号
Scheme sch;
sch.name = QString("方案%1").arg(counter++);
schemes_.push_back(sch);
schemesList_->addItem(sch.name);
schemesList_->setCurrentRow(schemes_.size() - 1);
```

#### 复制方案 (`duplicateScheme()`)
```cpp
Scheme* cur = currentScheme();
Scheme dup = *cur;  // 深拷贝所有 inputs 和 results
dup.name += "(副本)";
schemes_.push_back(dup);
schemesList_->addItem(dup.name);
schemesList_->setCurrentRow(schemes_.size() - 1);
```

#### 删除方案 (`removeScheme()`)
```cpp
if (schemes_.size() == 1) {
    QMessageBox::warning("至少保留一个方案");
    return;
}
int idx = currentSchemeIndex();
schemes_.removeAt(idx);
delete schemesList_->takeItem(idx);
```

#### 重命名方案 (`onSchemeItemDoubleClicked()`)
```cpp
int idx = currentSchemeIndex();
bool ok = false;
const QString newName = QInputDialog::getText(
    this, "编辑方案名称", "请输入新的方案名称：",
    QLineEdit::Normal, schemes_[idx].name, &ok);
if (ok && !newName.trimmed().isEmpty()) {
    schemes_[idx].name = newName.trimmed();
    schemesList_->item(idx)->setText(schemes_[idx].name);
}
```

**同步机制**: `schemes_` 和 `schemesList_` 的行号始终保持一致。

---

## 扩展开发指南

### 🎨 1. 添加新的项目类型

**步骤**:
1. 编辑 `SpecLoader.cpp` 中的 `kEmbeddedSpec` JSON
2. 按规范添加新项目定义

**示例 - 添加"风机设备投资"**:
```json
{
  "id": "turbine_capex",
  "group": "设备投资",
  "label": "风机设备",
  "unit": "万元",
  "inputs": [
    {"name": "unit_cost", "label": "单机成本", "type": "double", "unit": "万元/台", "required": true},
    {"name": "count", "label": "台数", "type": "int", "unit": "台", "required": true, "min": 1}
  ],
  "formula": "result = unit_cost * count;"
}
```

3. 如果需要新分组，同时在 `groups` 数组添加：
```json
"groups": ["变电部分", "海上变电部分", "陆上变电部分", "线路部分", "设备投资", ...]
```

---

### 🧮 2. 添加复杂计算逻辑

**方法一：使用 JavaScript 公式**（推荐）
```json
{
  "formula": "var temp = input1 * input2; result = Math.sqrt(temp) * 1.05;"
}
```

**方法二：注册 C++ 回调**（极复杂逻辑）
```cpp
// 在 Calculator 构造函数中注册
Calculator::Calculator() {
    registerCallback("complex_project", [](const ProjectSpec& spec, 
                                           const InputMap& inputs, 
                                           double& out, 
                                           QString* explain) -> bool {
        double a = inputs.value("param_a").toDouble();
        double b = inputs.value("param_b").toDouble();
        // 复杂计算逻辑...
        out = someComplexCalculation(a, b);
        if (explain) *explain = "自定义计算说明";
        return true;
    });
}
```

---

### 🎨 3. 自定义输入控件

**场景**: 需要日期选择器或自定义复合控件

**修改**: `EditorPanel::makeWidget()`
```cpp
QWidget* EditorPanel::makeWidget(const InputField& f, const QVariant& def) const {
    if (f.type == "date") {
        auto* picker = new QDateEdit;
        picker->setDate(def.toDate());
        return picker;
    }
    // ... 原有逻辑
}
```

同时在 `widgetValue()` 中添加对应的值获取逻辑：
```cpp
QVariant EditorPanel::widgetValue(const FieldWidget& fw) const {
    if (auto* picker = qobject_cast<QDateEdit*>(fw.w)) 
        return picker->date();
    // ... 原有逻辑
}
```

---

### 📊 4. 自定义汇总表样式

**修改**: `MainWindow::rebuildResultBody()`

**示例 - 添加条件格式**:
```cpp
auto* item = new QStandardItem(QString::number(val, 'f', 2));

// 根据数值大小设置颜色
if (val > 10000) {
    item->setForeground(QBrush(QColor(255, 0, 0)));  // 红色（超预算）
} else if (val < 5000) {
    item->setForeground(QBrush(QColor(0, 128, 0)));  // 绿色（节省）
}
```

---

### 💾 5. 支持外部 JSON 规格文件

**当前**: 规格硬编码在 `kEmbeddedSpec`

**改进**: 从文件加载
```cpp
ProjectSpecSet SpecLoader::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return loadDefault();  // 失败时降级到内嵌规格
    }
    QByteArray data = file.readAll();
    const auto doc = QJsonDocument::fromJson(data);
    // ... 解析逻辑同 loadDefault()
}
```

在 `MainWindow` 构造函数中调用：
```cpp
MainWindow::MainWindow(QWidget* parent): QMainWindow(parent) {
    spec_ = SpecLoader::loadFromFile("config/specs.json");
    if (spec_.items.isEmpty()) {
        spec_ = SpecLoader::loadDefault();  // 降级处理
    }
    // ...
}
```

---

### 📈 6. 添加图表可视化

**依赖**: `QT += charts` (需在 .pro 文件中添加)

**示例 - 柱状图对比**:
```cpp
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>

void MainWindow::showChart() {
    QBarSeries* series = new QBarSeries();
    
    for (const auto& sch : schemes_) {
        QBarSet* set = new QBarSet(sch.name);
        for (const auto& g : spec_.groupsInOrder) {
            double sum = 0.0;
            // 计算该分组的小计...
            *set << sum;
        }
        series->append(set);
    }
    
    QChart* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("方案对比");
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->show();
}
```

---

## 常见问题 (FAQ)

### ❓ 1. 公式中如何使用数学函数？

**答**: QJSEngine 支持所有 JavaScript Math 对象：
```javascript
result = Math.sqrt(input1);        // 平方根
result = Math.pow(input1, 2);      // 平方
result = Math.abs(input1);         // 绝对值
result = Math.max(a, b, c);        // 最大值
```

---

### ❓ 2. 如何处理可选输入字段？

**答**: 设置 `required: false`，在公式中检查：
```javascript
if (optional_input != undefined) {
    result = base * optional_input;
} else {
    result = base * 1.0;  // 默认系数
}
```

---

### ❓ 3. 汇总表能否支持分页或导出 Excel？

**答**: 
- **导出 Excel**: 需集成第三方库（如 QXlsx）
- **分页**: 可使用 `QTableView` 的滚动功能，或自定义分页控件

---

### ❓ 4. 如何调试公式计算错误？

**答**: 使用 `explain` 参数：
```cpp
QString explain;
double result;
if (calc_.evaluate(spec, inputs, result, &explain)) {
    qDebug() << explain;  // 查看计算过程
} else {
    qDebug() << "错误:" << explain;
}
```

---

### ❓ 5. 能否支持多语言？

**答**: 可以使用 Qt 的国际化机制：
```cpp
// 使用 tr() 包裹所有字符串
setWindowTitle(tr("方案管理器"));
actGen_ = tb->addAction(tr("生成汇总"));
```
然后使用 `lupdate` 和 `lrelease` 工具生成翻译文件。

---

## 代码规范与最佳实践

### ✅ 1. 命名约定
- **类名**: PascalCase (如 `MainWindow`)
- **成员变量**: camelCase + `_` 后缀 (如 `editor_`)
- **局部变量**: camelCase (如 `curScheme`)
- **常量**: kPascalCase (如 `kEmbeddedSpec`)

### ✅ 2. 信号槽连接
- 优先使用**新式语法** (函数指针)
- 示例: `connect(actGen_, &QAction::triggered, this, &MainWindow::onGenerate);`

### ✅ 3. 内存管理
- **父子关系**: 所有 Qt 控件通过父对象自动管理
- **手动删除**: 使用 `deleteLater()` 而非 `delete`

### ✅ 4. 数据同步
- `schemes_` 和 `schemesList_` 行号必须一致
- 修改数据后立即调用 `refresh*()` 方法更新 UI

---

## 性能优化建议

### ⚡ 1. 大规模方案优化
- 当方案数 > 50 时，考虑使用虚拟列表 (QAbstractItemModel)
- 延迟加载：仅计算当前可见方案

### ⚡ 2. 计算优化
- 缓存公式解析结果（避免重复 `QJSEngine::evaluate`）
- 并行计算：使用 `QtConcurrent::map` 批量计算

### ⚡ 3. UI响应优化
- 将耗时计算移到工作线程（QThread）
- 使用 `QProgressBar` 显示进度

---

## 总结

**WindPowerCalc** 是一个架构清晰、职责分离的 Qt 应用：
- **数据层** (`Spec`/`SpecLoader`) 负责规格定义
- **业务层** (`Calculator`) 负责计算逻辑
- **UI层** (`MainWindow`/`EditorPanel`) 负责交互展示

**核心优势**:
✅ **规格驱动**: 通过 JSON 配置快速扩展新项目  
✅ **实时计算**: 输入即时反馈计算结果  
✅ **多方案对比**: 直观的横向对比表  
✅ **灵活扩展**: 支持复杂公式和自定义控件  

**适用场景**:
- 风电投资评估
- 成本效益分析
- 工程方案比选
- 任何需要**规格化输入+公式计算+多方案对比**的场景

---

## 附录

### 📚 参考文档
- [Qt 6 官方文档](https://doc.qt.io/qt-6/)
- [QJSEngine 文档](https://doc.qt.io/qt-6/qjsengine.html)
- [JSON 格式规范](https://www.json.org/json-zh.html)

### 📧 联系方式
如有问题或建议，请通过以下方式联系：
- 项目路径: `E:\Qt_project\WindPowerCalc`
- 开发工具: Qt Creator 6.8.1

---

**文档版本**: v1.1  
**最后更新**: 2025年11月10日  
**作者**: AI 编程助手

