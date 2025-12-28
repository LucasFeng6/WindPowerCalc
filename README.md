# 海上风电全生命周期费用计算（WindPowerCalc）项目说明

本文档面向**第一次接手该项目的维护者**，帮助你快速理解项目结构、核心数据模型、计算逻辑和 UI 交互流程，并给出一些扩展/修改时的注意点。

---

## 1. 项目概览

- 技术栈：Qt 6（Widgets）、C++17、QJSEngine（内嵌 JS 表达式计算）、ActiveQt（导出 Excel）。
- 功能定位：针对海上风电工程，按“初期投资 + 年运行费用”的视角，计算不同方案的全生命周期费用，并支持：
  - 多个“方案”并行配置、对比；
  - 以分组树的形式填写各类费用参数；
  - 自动根据公式计算各项费用及分组/总计；
  - 将汇总结果导出到 Excel。
- 关键概念：
  - **ProjectSpec**：一个可计算的“项目项”（例如“海上换流站”、“直流海缆损耗”等）；
  - **InputField**：某一项目项下的一个输入字段（电阻、电流、长度、倍率等）；
  - **Scheme**：一套完整的输入配置（一个方案），包含所有项目项的输入值与计算结果；
  - **sharedInputs / sharedKey**：跨项目共享的输入（例如“电价 price”、“各类总投资”等）。

---

## 2. 代码结构总览

根目录下主要源文件：

- `main.cpp`：程序入口，创建 `QApplication` 并显示 `MainWindow`。
- `MainWindow.h/.cpp`：主窗口，负责：
  - 整体 UI 布局（方案列表、项目树、右侧编辑区域、下方结果表）；
  - 方案的新增/复制/删除/重命名；
  - 与 `EditorPanel`、`Calculator` 协同，进行输入保存与费用计算；
  - 汇总结果生成、经济参数设置、方案集保存/加载、Excel 导出等。
- `EditorPanel.h/.cpp`：右侧“编辑栏”，根据 `ProjectSpec` 动态生成表单，负责：
  - 展示项目的输入项、单位、备注；
  - 必填校验、默认值处理、按回车跳转到下一项；
  - 将表单内容采集回 `MainWindow`。
- `Spec.h/.cpp`：纯数据结构定义：
  - `InputField`、`ProjectSpec`、`ProjectSpecSet` 等。
- `SpecLoader.h/.cpp`：规格加载：
  - 内嵌 JSON（`kEmbeddedSpec`）定义所有项目、分组、输入字段与计算公式；
  - 启动时解析 JSON，得到 `ProjectSpecSet`。
- `Calculator.h/.cpp`：计算引擎封装：
  - 使用 `QJSEngine` 执行 `ProjectSpec::formula`；
  - 将输入值注入 JS 全局变量中，读取 `result` 作为输出。

工程配置文件：

- `WindPowerCalc.pro`：Qt 工程文件，声明源/头文件及依赖模块（`core`, `gui`, `widgets`, `qml`, `axcontainer` 等）。

构建输出放在：

- `build/`、`build-debug/`：由 Qt Creator 生成的构建目录，可忽略逻辑分析。

---

## 3. 核心数据模型（Spec / Scheme）

### 3.1 输入字段 `InputField`

定义于 `Spec.h`：

```cpp
struct InputField {
    QString name;
    QString label;
    QString unit;     // e.g. "km", "Ω/km"
    QString sharedKey; // optional: fields with same sharedKey share one value across projects
    QVariant defval;
    bool required = false;
    double min = std::numeric_limits<double>::lowest();
    double max = std::numeric_limits<double>::max();
};
```

要点：

- `name`：在 JS 公式里使用的变量名。
- `label`：表单中显示给用户的名称。
- `unit`：单位，用于 UI 展示。
- `sharedKey`：
  - 为空：该字段只属于当前项目项（存入 `Scheme::inputs[projId][name]`）；
  - 非空：该字段与其它项目共享（存入 `Scheme::sharedInputs[sharedKey]`）。
- `defval`：默认值（如默认的电价、倍率等）。
- `required`：若为 true，编辑界面会校验必填。
- `min / max`：数值限制，用于 `QDoubleValidator` 进行前端校验。

### 3.2 项目项 `ProjectSpec`

```cpp
struct ProjectSpec {
    QString id;       // unique
    QString group;    // 分组
    QString label;    // 显示名
    QString unit;     // 结果单位（如：万元/年）
    QVector<InputField> inputs;
    QString formula;  // QJSEngine 表达式（可空，若注册了回调）
    QString note;     // 备注
    bool groupHeader = false; // 若为分组标题行（用于汇总表美化）
};
```

要点：

- `id`：程序内部用来识别某一项目（例如 `"offshore_converter"`）。
- `group`：项目所在的业务分组，例如 `"海上变电部分"`, `"损耗费用"` 等。
- `unit`：使用于结果展示和汇总（如 `万元`、`万元/年`）。
- `inputs`：该项目的所有输入字段。
- `formula`：使用 QJSEngine 计算的 JS 代码，需要向 `result` 写入最终计算值。

### 3.3 项目集 `ProjectSpecSet`

```cpp
struct ProjectSpecSet {
    QVector<ProjectSpec> items;           // 含分组内顺序
    QStringList groupsInOrder;            // 展示次序
    QMap<QString, QVector<int>> groupRows;// group -> indices
};
```

由 `SpecLoader::loadDefault()` 在程序启动时构建，用于：

- 建 UI 的项目树（`MainWindow::buildProjectModel`）；
- 在汇总表中按分组、按顺序输出明细。

### 3.4 方案 `Scheme`

定义于 `MainWindow.h`：

```cpp
struct Scheme {
    QString name;
    QMap<QString, QMap<QString,QVariant>> inputs; // projId -> (field -> value)
    QMap<QString, double> results;                // projId -> result
    QMap<QString, QVariant> sharedInputs;         // sharedKey -> value
};
```

含义：

- `inputs`：每个项目项自己的输入值。
- `sharedInputs`：跨项目复用的值，比如：
  - `"price"`：电价，在各类损耗/停运费用中使用；
  - `"offshore_capex_total"`, `"core_capex_total"` 等：各分组总投资，用于其他费用/维护费。
- `results`：每个项目项的计算结果（统一以 double 存储）。

`MainWindow` 维护 `QVector<Scheme> schemes_`，与左侧方案列表一一对应。

---

## 4. 配置与公式：`SpecLoader` 与内嵌 JSON

`SpecLoader.cpp` 内定义了一个较长的 JSON 字符串 `kEmbeddedSpec`，包含所有项目的定义。启动时：

```cpp
const auto doc = QJsonDocument::fromJson(QByteArray(kEmbeddedSpec));
const auto obj = doc.object();
for (const auto& v : obj.value("groups").toArray())
    set.groupsInOrder << v.toString();

const auto items = obj.value("items").toArray();
for (const auto& v : items) {
    set.items.push_back(parseItem(v.toObject()));
}
for (int i=0;i<set.items.size();++i) {
    set.groupRows[ set.items[i].group ].push_back(i);
}
```

示例：变电设备维护费 `om_substation_equipment` 的配置（节选）：

```json
{
  "id":"om_substation_equipment",
  "group":"维护费",
  "label":"变电设备维护费",
  "unit":"万元/年",
  "inputs":[
    {"name":"offshore_rate","label":"海上设备折算倍率","type":"double","unit":"倍","required":false,"defval":0.02},
    {"name":"onshore_rate","label":"陆上设备折算倍率","type":"double","unit":"倍","required":false,"defval":0.02}
  ],
  "formula":"result = (offshore_capex_total * offshore_rate) + (onshore_capex_total * onshore_rate);"
}
```

要点：

- `formula` 直接写 JS 表达式，**能访问所有输入字段与共享输入**：
  - 例如上例中的 `offshore_capex_total` 和 `onshore_capex_total` 是共享输入，由 `updateCapexTotals()` 在计算流程中填入。
- 某些字段有 `"sharedKey": "price"`：
  - 表示这些输入项都读写同一个共享键 `"price"`，从而用户只需在任一处修改电价，其他项目都会使用最新值。

扩展方式：

- 增加新项目：
  - 在 `items` 数组中添加一个 JSON 对象；
  - 设置合理的 `id`、`group`、`label`、`unit`、`inputs`、`formula`；
  - 若需要参与汇总，需要确保 `group` 在 `groups` 列表中出现，并在 `initialInvestGroups` 或 `annualCostGroups` 中包含对应分组名称。

---

## 5. 计算引擎 `Calculator`

核心逻辑在 `Calculator::evaluate`（`Calculator.cpp`）：

```cpp
bool Calculator::evaluate(const ProjectSpec& spec,
                          const InputMap& inputs,
                          double& out) const {
    QJSEngine eng;
    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
        eng.globalObject().setProperty(it.key(), QJSValue(it.value().toDouble()));
    }
    eng.globalObject().setProperty("result", 0.0);

    const QString code = spec.formula;
    const QJSValue r = eng.evaluate(code);
    if (r.isError()) {
        return false;
    }
    const QJSValue rv = eng.globalObject().property("result");
    if (!rv.isNumber()) {
        return false;
    }
    out = rv.toNumber();
    return true;
}
```

使用方式：

- 调用方负责构造 `inputs`（包含本项目输入 + 共享输入）；
- `formula` 可以是任意 JS 代码，只要最后在 `result` 上写入数值即可；
- 若公式执行出错或 `result` 非数值，则返回 `false`，上层会将该条结果视为“计算失败”。

---

## 6. 主窗口与 UI 逻辑：`MainWindow`

### 6.1 界面布局（`initUi`）

`MainWindow::initUi()` 搭建整体界面：

- 顶部工具栏：
  - `新增方案`（`addScheme`）、`复制方案`（`duplicateScheme`）、`删除方案`（`removeScheme`）；
  - `生成汇总`（`onGenerate`）；
  - `导出Excel`（`onExportExcel`）；
  - `保存方案集`（`onSave`）/`加载方案集`（`onLoad`）；
  - `经济参数`（`onEditEconomicParams`）。
- 中部垂直分割为上下两块：
  - 上半：三列水平分割：
    - 左：方案列表（`QListWidget* schemesList_`）；
    - 中：项目树（`QTreeView* projectView_` + `QStandardItemModel* projectModel_`）；
    - 右：参数编辑面板（`EditorPanel* editor_`）。
  - 下半：结果表格（`QTableView* resultView_` + `QStandardItemModel* resultModel_`）。

### 6.2 项目树与分组汇总（`buildProjectModel`）

- 使用 `spec_.groupsInOrder` 和 `groupRows` 构建树形结构：
  - 顶级标题行：`初期投资`、`年运行费`；
  - 子分组行：例如 `"海上变电部分"`, `"陆上变电部分"`, `"损耗费用"` 等；
  - 分组下为具体项目项（如“海上换流站”、“海缆费用”等）。
- `itemSummaryItems_`：记录每个项目项右侧“状态/小计”列的 `QStandardItem*`；在刷新时更新显示：
  - 未填写 → `"未填写"`；
  - 有输入但计算失败 → `"已填写（计算失败）"`；
  - 计算成功 → 显示数值。
- `groupSummaryItems_`：每个分组行右侧的汇总单元，用于显示该分组的小计（万元 / 万元/年）。
- `initialSummaryItem_` / `annualSummaryItem_`：分别显示“初期投资”与“年运行费”的总体汇总。

刷新逻辑在 `refreshProjectSummaries()` 中完成。

### 6.3 方案切换与输入编辑流程

1. 用户在左侧方案列表切换当前方案：
   - `onSchemeListChanged` 被调用：
     - 调用 `refreshProjectSummaries()` 刷新项目树状态；
     - 调用 `onProjectSelectionChanged()` 根据当前选中的项目更新右侧编辑面板。
2. 用户在项目树中选择项目：
   - `onProjectSelectionChanged()`：
     - 根据当前 `QModelIndex` 找到对应的 `ProjectSpec`；
     - 从 `currentScheme()->inputs` 和 `currentScheme()->sharedInputs` 中取出该项目已保存的输入；
     - 调用 `editor_->setProject()` 构建表单；
     - 调用 `editor_->focusFirstField()` 聚焦到首个可编辑字段。
3. 用户修改表单内容：
   - `EditorPanel` 在每个 `QLineEdit` 的 `textChanged` 信号上连接 `MainWindow::onEditChanged()`。
   - `onEditChanged()`：
     - 调用 `editor_->collectInputs(ownInputs, sharedInputs, &err)` 获取用户输入，进行必填校验；
     - 将 `ownInputs` 写回 `Scheme::inputs[projId]`；
     - 将 `sharedInputs` 合并到 `Scheme::sharedInputs`。

### 6.4 计算流程与共享输入

`onEditChanged()` 中的核心逻辑：

1. 合并输入（当前项目）：

   ```cpp
   auto mergedInputs = [&](const QString& projId) {
       QMap<QString,QVariant> all = sch->sharedInputs;
       const auto own = sch->inputs.value(projId);
       for (auto it = own.begin(); it != own.end(); ++it) {
           all[it.key()] = it.value();
       }
       return all;
   };
   ```

2. 计算当前项目：
   - 调用 `calc_.evaluate(*spec, mergedInputs(spec->id), result)`；
   - 成功则写入 `sch->results[spec->id]`，失败则移除该 key。

3. 共享输入变化后，重新计算其他已填写项目：
   - 遍历 `spec_.items`，对已填写过（有输入）的项目重新调用 `evaluate`。

4. 更新基于总投资的共享输入：
   - 调用 `updateCapexTotals(spec_, sch)`：
     - 遍历所有项目结果，按分组累计：
       - `"海上变电部分"` → `offshore_capex_total`
       - `"陆上变电部分"` → `onshore_capex_total`
       - `"线路部分"` → `line_capex_total`
       - `"其他设备"` → `other_equipment_capex_total`
     - 计算 `core_capex_total = 上述四项之和`；
     - 将这些值写入 `sch->sharedInputs[...]`。

5. 在新的总投资基础上重新计算特别项目：
   - 变电设备维护费（`id == "om_substation_equipment"`）：
     - 若用户未编辑过该项目，则用默认倍率初始化输入；
     - 使用更新后的 `offshore_capex_total` / `onshore_capex_total` 重新计算。
   - 其他费用（`id == "other_cost"`）：
     - 若未编辑，使用默认折算倍率；
     - 使用更新后的 `core_capex_total` 重算。

6. 最后调用 `refreshProjectSummaries()` 更新项目树中的状态与分组小计。

---

## 7. 汇总结果与年费用计算

### 7.1 生成汇总（`onGenerate`）

- 用户点击“生成汇总”：
  - 调用 `rebuildResultHeader()` 创建结果表的列头：
    - `项目` | `单位` | 每个方案的列。
  - 调用 `rebuildResultBody()` 填充内容：
    - 按 `ProjectSpecSet` 的分组/顺序输出分组标题、分组小计、每个项目在各方案下的数值。
  - 标记 `hasSummary_ = true`。

### 7.2 明细与分组小计行（`rebuildResultBody`）

`rebuildResultBody()` 主要逻辑：

- 为“初期投资”、“年运行费”分别添加一个标题行，记录其对应的“总计”单元格，用于后续写入总额。
- 对每个分组 g：
  - 求每个方案在 g 分组下所有项目结果的和，填入分组小计行；
  - 再一行一行写入该分组下各项目的明细。
- 最后将各分组小计累加，得到每个方案的：
  - `initialInvestTotals[i]`：初期投资总额；
  - `annualCostTotals[i]`：年运行费用总额。

### 7.3 年费用（等额年金）计算

在 `rebuildResultBody()` 的末尾，计算每个方案的 **年费用**：

```cpp
const double powTerm = std::pow(1.0 + recoveryRate_, serviceYears_);
const double denominator = powTerm - 1.0;
double annuityFactor = 0.0;
if (std::abs(denominator) > 1e-9) {
    annuityFactor = (recoveryRate_ * powTerm) / denominator;
}
for (int i = 0; i < schemes_.size(); ++i) {
    annualFeeTotals[i] = initialInvestTotals[i] * annuityFactor + annualCostTotals[i];
}
```

含义：

- 将一次性初期投资按“投资回报率 `recoveryRate_`”和“使用年限 `serviceYears_`”折算为等额年金；
- 再加上年运行费用，得到每年等效费用。
- 最后在表格底部添加两行总计：
  - `初期投资`（万元）；
  - `年费用`（万元/年）。

---

## 8. 方案集保存 / 加载

### 8.1 保存（`onSave`）

- 用户点击“保存方案集”后，选择一个 JSON 文件路径；
- 程序序列化以下内容：
  - `recoveryRate_`、`serviceYears_`；
  - 对每个 `Scheme`：
    - `name`；
    - `inputs`：按 `projId -> { fieldName: value }` 序列化；
    - `results`：按 `projId -> result` 序列化；
    - `sharedInputs`：按 `sharedKey -> value` 序列化。
- 写入 JSON 文件。

### 8.2 加载（`onLoad`）

- 用户选择 JSON 文件：
  - 解析根对象，读取 `recoveryRate`、`serviceYears`；
  - 清空当前 `schemes_` 和 `schemesList_`；
  - 对每个方案对象：
    - 恢复 `name`、`inputs`、`sharedInputs`、`results`；
    - 加入 `schemes_`，并往左侧方案列表中插入对应项。
- 若加载结果为空，则自动创建一个空方案。
- 最后选中第一个方案，并更新状态栏提示。

---

## 9. Excel 导出（`onExportExcel`）

- 前提：仅在 Windows + 安装了 Microsoft Excel 的环境下可用（使用 ActiveQt `QAxObject`）。
- 处理流程：
  1. 若没有方案则提示；
  2. 调用 `onGenerate()` 确保结果表是最新的；
  3. 弹出保存对话框，获取 `.xlsx` 路径；
  4. 创建 Excel.Application COM 对象，关闭可见性和警告；
  5. 新建工作簿和工作表；
  6. 将 `resultModel_` 中的表头和每一行数据写入 Excel 单元格；
  7. 调用 `AutoFit()` 自动调整列宽；
  8. 保存并关闭工作簿，退出 Excel；
  9. 更新状态栏，并弹出“导出成功”提示。

维护注意：

- 若将来打算在无 Excel 的环境（例如 Linux）下也导出，需要更换为纯 C++ 的 xlsx 库（项目中之前使用过 `SimpleXlsxWriter`，但当前源码未保留，只存在编译产物）。

---

## 10. EditorPanel 细节与表单交互

`EditorPanel` 负责单个项目的表单渲染与输入采集。

### 10.1 构建表单（`setProject`）

- 根据传入的 `ProjectSpec`：
  - 设置标题 `title_`（如：`海上换流站（万元）`）；
  - 显示备注 `note_`；
  - 遍历 `spec.inputs`，为每个 `InputField` 构建一行：
    - 右侧是一个 `QLineEdit` + 单位 `QLabel`；
    - 左侧是字段 label（必填项增加 `*` 标记）。
- 默认值逻辑：
  - 如果当前方案已经保存了该字段值，则显示保存值；
  - 否则，如果 `defval` 有意义且非 0，则显示默认值（例如默认倍率、电价等）。

### 10.2 校验与采集（`collectInputs`）

`collectInputs(QMap<QString,QVariant>& ownOut,
              QMap<QString,QVariant>& sharedOut, QString* err)`：

- 遍历 `fields_`：
  - 调用 `checkRequired`，若必填项为空则返回 `false` 并在 `err` 中写明哪个字段未填；
  - 空字符串但有默认值 → 使用默认值；
  - 根据 `sharedKey` 决定写入 `ownOut` 还是 `sharedOut`。
- 成功返回 `true`。

### 10.3 键盘操作（`eventFilter`）

- 监听 `QLineEdit` 的回车键：
  - 按回车时自动切换到下一个输入框，并全选内容，方便连续输入。

---

## 11. 扩展与维护建议

### 11.1 新增/修改项目项

1. 在 `SpecLoader.cpp` 的 `kEmbeddedSpec` 中添加或修改 JSON 项目：
   - 确保 `id` 唯一；
   - 设置合理的 `group`（若要在 UI 中出现，请同时在 `groups` 列表中维护该分组名称）；
   - 根据需要设置 `inputs`、`defval`、`sharedKey` 等；
   - 编写正确的 `formula`（参考已有公式）。
2. 如果新分组属于“初期投资”或“年运行费”，需要在 `MainWindow.cpp` 顶部的：
   - `initialInvestGroups()` 或 `annualCostGroups()` 中增加对应分组名称；
   - 这样才能正确参与树状项目和汇总表的统计。

### 11.2 新增共享输入

1. 某些新公式需要使用“整体量”（例如“所有线路总投资”）：
   - 在 `updateCapexTotals()` 中增加统计逻辑，计算并写入新的 `sch->sharedInputs[...]`；
   - 在 JSON 公式中直接使用该全局变量名，如 `"line_capex_total"`, `"core_capex_total"`。
2. 若只是希望同一数值被多个项目共享（例如电价）：
   - 在 `InputField` 的 JSON 中设置相同的 `"sharedKey"`；
   - EditorPanel 会自动将它们视为共享值，输入一次即可在多处复用。

### 11.3 重要注意点

- **不要直接在多个地方硬编码公式**：
  - 所有业务公式尽量保持在 `SpecLoader.cpp` 的 JSON 中；
  - C++ 侧仅作为通用执行器，避免逻辑分散。
- **编辑 JSON 后要重新编译**：
  - 因为 JSON 被编译为内嵌字符串，修改后需要重新构建工程。
- **数值稳定性**：
  - 年金系数计算中有除法，已通过 `std::abs(denominator) > 1e-9` 做了简单防护；
  - 若将来允许极端参数（如非常小的投资回报率或超长年限），可考虑进一步数值稳定性处理。

---

## 12. 上手建议

若你要快速开始修改/扩展：

1. 先阅读 `SpecLoader.cpp` 中的 JSON，理解各项目和公式的业务含义；
2. 在界面上尝试录入几组数据，观察项目树和汇总表的变化，对应代码中的 `onEditChanged()` 与 `refreshProjectSummaries()`；
3. 修改某个简单项目的公式或默认值，重新编译并验证 UI 行为；
4. 若需要新增复杂项目，可在 JSON 中先复制一个相似项目并调整字段/公式，再根据需要修改 `updateCapexTotals()` 或分组列表。

如需我帮你进一步梳理某个具体分组或公式的业务含义，也可以在问题里指出对应 `id` 或界面名称。
